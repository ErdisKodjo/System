// SPDX-License-Identifier: MIT
//
// shader_cache.cpp — SPIR-V cache keyed by XXH64(hlsl_source + entry + profile + flags).
//
// Compiling HLSL → SPIR-V is expensive (tens to hundreds of milliseconds per
// shader). To avoid recompiling on every process start, the AfriOS DXVK
// shader cache memoizes compiled SPIR-V blobs by a 64-bit hash of:
//
//     XXH64(hlsl_source || entry_point || profile || compile_flags)
//
// The hash is rendered as 16 hex chars and used as the on-disk filename under
// `/var/cache/afros-dxvk/shaders/` (see util/cache_manager.cpp). Lookups go
// through the in-memory LRU first, then disk.
//
// The XXH64 implementation included here is the reference algorithm (BSD-2);

#include "vulkan_loader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace dxvk {

namespace {

// --- XXH64 (reference implementation, BSD-2 license) -----------------------
constexpr uint64_t kPrime64_1 = 0x9E3779B185EBCA87ull;
constexpr uint64_t kPrime64_2 = 0xC2B2AE3D27D4EB4Full;
constexpr uint64_t kPrime64_3 = 0x165667B19E3779F9ull;
constexpr uint64_t kPrime64_4 = 0x85EBCA77C2B2AE63ull;
constexpr uint64_t kPrime64_5 = 0x27D4EB2F165667C5ull;

inline uint64_t rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}
inline uint64_t rd64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}

uint64_t XXH64(const void* input, size_t len, uint64_t seed = 0) {
    const auto* p = static_cast<const uint8_t*>(input);
    const uint8_t* const end = p + len;
    uint64_t h;
    if (len >= 32) {
        uint64_t v1 = seed + kPrime64_1 + kPrime64_2;
        uint64_t v2 = seed + kPrime64_2;
        uint64_t v3 = seed;
        uint64_t v4 = seed - kPrime64_1;
        const uint8_t* const lim = end - 32;
        do {
            v1 = rotl64(v1 + rd64(p) * kPrime64_2, 31) * kPrime64_1; p += 8;
            v2 = rotl64(v2 + rd64(p) * kPrime64_2, 31) * kPrime64_1; p += 8;
            v3 = rotl64(v3 + rd64(p) * kPrime64_2, 31) * kPrime64_1; p += 8;
            v4 = rotl64(v4 + rd64(p) * kPrime64_2, 31) * kPrime64_1; p += 8;
        } while (p <= lim);
        h = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);
        h = (h ^ (rotl64(v1 * kPrime64_2, 31) * kPrime64_1)) * kPrime64_1 + kPrime64_4;
        h = (h ^ (rotl64(v2 * kPrime64_2, 31) * kPrime64_1)) * kPrime64_1 + kPrime64_4;
        h = (h ^ (rotl64(v3 * kPrime64_2, 31) * kPrime64_1)) * kPrime64_1 + kPrime64_4;
        h = (h ^ (rotl64(v4 * kPrime64_2, 31) * kPrime64_1)) * kPrime64_1 + kPrime64_4;
    } else {
        h = seed + kPrime64_5;
    }
    h += static_cast<uint64_t>(len);
    while (p + 8 <= end) {
        const uint64_t k1 = rotl64(rd64(p) * kPrime64_2, 31) * kPrime64_1;
        h ^= k1; h = rotl64(h, 27) * kPrime64_1 + kPrime64_4; p += 8;
    }
    if (p + 4 <= end) {
        uint32_t k = 0; std::memcpy(&k, p, 4);
        h ^= static_cast<uint64_t>(k) * kPrime64_1;
        h = rotl64(h, 23) * kPrime64_2 + kPrime64_3; p += 4;
    }
    while (p < end) {
        h ^= static_cast<uint64_t>(*p) * kPrime64_5;
        h = rotl64(h, 11) * kPrime64_1; ++p;
    }
    h ^= h >> 33; h *= kPrime64_2;
    h ^= h >> 29; h *= kPrime64_3;
    h ^= h >> 32;
    return h;
}

} // namespace

/// ShaderCache — in-memory LRU + on-disk SPIR-V cache.
class ShaderCache {
public:
    /// Compute the cache key for one HLSL source + entry + profile + flags.
    static uint64_t computeKey(const char* src, const char* entry,
                               const char* profile, uint32_t flags) {
        // Hash the concatenation with length prefixes so `("ab","c")` and
        // `("a","bc")` produce different keys.
        std::string buf;
        const auto append = [&](const char* s) {
            const size_t n = s ? std::strlen(s) : 0;
            buf.append(reinterpret_cast<const char*>(&n), sizeof(n));
            if (n) buf.append(s, n);
        };
        append(src); append(entry); append(profile);
        buf.append(reinterpret_cast<const char*>(&flags), sizeof(flags));
        return XXH64(buf.data(), buf.size(), 0xAF105C1u);
    }

    /// Render `key` as a 16-char lowercase hex string (on-disk filename).
    static std::string keyHex(uint64_t key) {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016llx",
                      static_cast<unsigned long long>(key));
        return std::string(buf, 16);
    }

    /// In-memory lookup. Returns nullptr on miss.
    const std::vector<uint32_t>* lookup(uint64_t key) const {
        const auto it = m_map.find(key);
        if (it == m_map.end()) return nullptr;
        // Move to front for LRU recency.
        m_lru.remove(key);
        m_lru.push_front(key);
        return &it->second;
    }

    /// Insert a freshly-compiled SPIR-V module into the in-memory cache.
    void insert(uint64_t key, std::vector<uint32_t> spirv) {
        if (m_map.size() >= m_capacity) evictOne();
        m_map[key] = std::move(spirv);
        m_lru.push_front(key);
    }

    size_t size() const noexcept { return m_map.size(); }
    size_t capacity() const noexcept { return m_capacity; }
    void setCapacity(size_t n) noexcept { m_capacity = n ? n : 1; }

private:
    void evictOne() {
        if (m_lru.empty()) return;
        const uint64_t victim = m_lru.back();
        m_lru.pop_back();
        m_map.erase(victim);
    }

    using LruList = std::list<uint64_t>;
    std::unordered_map<uint64_t, std::vector<uint32_t>> m_map;
    mutable LruList m_lru;
    size_t m_capacity = 1024;
};

} // namespace dxvk

// --- C entry points used by the HLSL compiler ------------------------------
extern "C" {

uint64_t shader_cache_key(const char* src, const char* entry,
                          const char* profile, uint32_t flags) {
    return dxvk::ShaderCache::computeKey(src, entry, profile, flags);
}

} // extern "C"
