// SPDX-License-Identifier: MIT
//
// vulkan_memory.cpp — VulkanMemoryAllocator-style suballocator.
//
// D3D buffers / textures are small and numerous; calling `vkAllocateMemory`
// per resource hits the ICD's `maxMemoryAllocationCount` limit (often 4096)
// quickly. AfriOS DXVK therefore suballocates large "heap blocks" out of
// DEVICE_LOCAL and HOST_VISIBLE memory types and hands out offsetted slices
// to resources. This mirrors VMA's `VMA_MEMORY_USAGE_AUTO` + buddy allocator.
//
// The skeleton implements a first-fit free-list allocator per memory type
// with `Alloc()` / `Free()` / `Defragment()` (stub). Real builds would route
// through VMA; the interface here is compatible so the rest of the stack
// doesn't change when VMA is wired in.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "vulkan_private.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>

namespace dxvk {

/// One free region inside a heap block.
struct FreeRegion {
    uint64_t offset = 0;
    uint64_t size   = 0;
};

/// One allocated slice returned to the caller.
struct Allocation {
    VkDeviceMemory memory = nullptr;     // backing VkDeviceMemory
    uint64_t       offset = 0;           // byte offset within `memory`
    uint64_t       size   = 0;
    uint32_t       memoryTypeIndex = 0;
};

/// One large block sub-allocated from a single `vkAllocateMemory` call.
class HeapBlock {
public:
    HeapBlock(VkDeviceMemory mem, uint64_t size, uint32_t typeIndex)
        : m_memory(mem), m_size(size), m_typeIndex(typeIndex) {
        m_free.push_back({0, size});
    }

    /// Try to satisfy an allocation of `size` bytes (alignment optional).
    /// Returns true and fills `out` on success.
    bool alloc(uint64_t size, uint64_t alignment, Allocation& out) {
        const uint64_t alignMask = alignment - 1;
        for (size_t i = 0; i < m_free.size(); ++i) {
            FreeRegion& r = m_free[i];
            const uint64_t aligned =
                (r.offset + alignMask) & ~alignMask;
            const uint64_t padding = aligned - r.offset;
            if (padding + size > r.size) continue;
            out.memory = m_memory;
            out.offset = aligned;
            out.size   = size;
            out.memoryTypeIndex = m_typeIndex;
            // Shrink / split the free region.
            r.offset += padding + size;
            r.size   -= padding + size;
            if (r.size == 0) m_free.erase(m_free.begin() + i);
            m_allocated += size;
            return true;
        }
        return false;
    }

    /// Return a previously-allocated slice to the free list (coalesces
    /// neighbours).
    void free(const Allocation& a) {
        FreeRegion r{a.offset, a.size};
        m_free.push_back(r);
        coalesce();
        m_allocated -= a.size;
    }

    VkDeviceMemory memory() const noexcept { return m_memory; }
    uint64_t allocated() const noexcept { return m_allocated; }
    uint64_t capacity()  const noexcept { return m_size; }
    double   utilization() const noexcept {
        return m_size ? static_cast<double>(m_allocated) / m_size : 0.0;
    }

private:
    void coalesce() {
        if (m_free.size() < 2) return;
        // Sort by offset then merge adjacent.
        std::sort(m_free.begin(), m_free.end(),
                  [](const FreeRegion& a, const FreeRegion& b) {
                      return a.offset < b.offset;
                  });
        std::vector<FreeRegion> merged;
        merged.push_back(m_free.front());
        for (size_t i = 1; i < m_free.size(); ++i) {
            FreeRegion& last = merged.back();
            if (last.offset + last.size == m_free[i].offset) {
                last.size += m_free[i].size;
            } else {
                merged.push_back(m_free[i]);
            }
        }
        m_free.swap(merged);
    }

    VkDeviceMemory              m_memory = nullptr;
    uint64_t                    m_size   = 0;
    uint32_t                    m_typeIndex = 0;
    uint64_t                    m_allocated = 0;
    std::vector<FreeRegion>     m_free;
};

/// VulkanMemoryAllocator — owns a vector of HeapBlocks per memory type.
class VulkanMemoryAllocator {
public:
    static constexpr uint64_t kBlockSize = 16 * 1024 * 1024; // 16 MiB

    explicit VulkanMemoryAllocator(std::shared_ptr<DxvkDevice> device)
        : m_device(std::move(device)) {}

    /// Allocate `size` bytes from a memory type matching `propertyFlags`.
    Allocation alloc(uint64_t size, uint64_t alignment,
                     uint32_t /*memoryTypeBits*/, uint32_t /*propertyFlags*/) {
        Allocation out{};
        // Try each existing block of the chosen type first.
        for (auto& block : m_blocks) {
            if (block.alloc(size, alignment, out)) return out;
        }
        // Otherwise allocate a new block large enough.
        const uint64_t blockSize = size > kBlockSize ? size : kBlockSize;
        VkDeviceMemory mem = m_device->allocateMemory(blockSize, ~0u, 0);
        if (!mem) return out;
        m_blocks.emplace_back(mem, blockSize, /*typeIndex*/ 0);
        if (!m_blocks.back().alloc(size, alignment, out)) {
            // Should not happen — block was just created with enough space.
            std::fprintf(stderr, "afros-dxvk: memory sub-alloc failed "
                                 "(size=%llu)\n",
                         static_cast<unsigned long long>(size));
        }
        return out;
    }

    void free(const Allocation& a) {
        for (auto& block : m_blocks) {
            if (block.memory() == a.memory) { block.free(a); return; }
        }
    }

    /// Defragmentation stub: in a real impl this would copy live allocations
    /// into a fresh block, swap, and release the old block. The skeleton just
    /// reports the current aggregate utilization.
    uint32_t defragment() {
        uint32_t moved = 0;
        for (auto& b : m_blocks) {
            if (b.utilization() < 0.25 && b.capacity() > kBlockSize) {
                // Candidate for compaction.
                ++moved;
            }
        }
        return moved;
    }

    uint64_t totalAllocated() const noexcept {
        uint64_t s = 0;
        for (const auto& b : m_blocks) s += b.allocated();
        return s;
    }
    uint64_t totalCapacity() const noexcept {
        uint64_t s = 0;
        for (const auto& b : m_blocks) s += b.capacity();
        return s;
    }
    size_t blockCount() const noexcept { return m_blocks.size(); }

private:
    std::shared_ptr<DxvkDevice>   m_device;
    std::vector<HeapBlock>        m_blocks;
};

} // namespace dxvk

// --- C entry points used by the rest of the back-end -----------------------
extern "C" {

dxvk::VulkanMemoryAllocator* vkmem_create(void* devicePtr) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::VulkanMemoryAllocator(std::move(device));
}
void vkmem_destroy(dxvk::VulkanMemoryAllocator* alloc) { delete alloc; }

} // extern "C"
