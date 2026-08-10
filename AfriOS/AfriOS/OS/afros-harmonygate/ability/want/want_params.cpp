/**
 * @file want_params.cpp
 * @brief AfriOS HarmonyOS compatibility — WantParams key-value bundle.
 *
 * WantParams is the parameter container carried by a Want. It stores
 * strongly-typed values keyed by string: int32, int64, string, bool, and
 * byte arrays. Designed to be serialisable so it can travel across the
 * SoftBus and across process boundaries.
 */

#include "afros_harmony.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace afros_harmony {

/** Supported value types. */
enum class ParamType : uint8_t {
    NONE   = 0,
    INT32  = 1,
    INT64  = 2,
    STRING = 3,
    BOOL   = 4,
    BYTES  = 5,
};

/** A single parameter value (tagged union). */
struct ParamValue {
    ParamType type = ParamType::NONE;
    int32_t    i32 = 0;
    int64_t    i64 = 0;
    bool        b  = false;
    std::string s;
    std::vector<uint8_t> bytes;
};

/** Key-value bundle. */
class WantParams {
public:
    WantParams() = default;

    int32_t SetInt32(const std::string &key, int32_t v)
    {
        std::lock_guard<std::mutex> g(lock_);
        ParamValue &pv = map_[key];
        pv.type = ParamType::INT32; pv.i32 = v;
        return AFROS_SUCCESS;
    }

    int32_t SetInt64(const std::string &key, int64_t v)
    {
        std::lock_guard<std::mutex> g(lock_);
        ParamValue &pv = map_[key];
        pv.type = ParamType::INT64; pv.i64 = v;
        return AFROS_SUCCESS;
    }

    int32_t SetString(const std::string &key, const std::string &v)
    {
        std::lock_guard<std::mutex> g(lock_);
        ParamValue &pv = map_[key];
        pv.type = ParamType::STRING; pv.s = v;
        return AFROS_SUCCESS;
    }

    int32_t SetBool(const std::string &key, bool v)
    {
        std::lock_guard<std::mutex> g(lock_);
        ParamValue &pv = map_[key];
        pv.type = ParamType::BOOL; pv.b = v;
        return AFROS_SUCCESS;
    }

    int32_t SetBytes(const std::string &key, const uint8_t *data, uint32_t len)
    {
        if (data == nullptr && len > 0) {
            return AFROS_ERROR_INVALID_PARAM;
        }
        std::lock_guard<std::mutex> g(lock_);
        ParamValue &pv = map_[key];
        pv.type = ParamType::BYTES;
        pv.bytes.assign(data, data + len);
        return AFROS_SUCCESS;
    }

    int32_t GetInt32(const std::string &key, int32_t *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = map_.find(key);
        if (it == map_.end() || it->second.type != ParamType::INT32) {
            return AFROS_ERROR;
        }
        if (out) *out = it->second.i32;
        return AFROS_SUCCESS;
    }

    int32_t GetInt64(const std::string &key, int64_t *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = map_.find(key);
        if (it == map_.end() || it->second.type != ParamType::INT64) {
            return AFROS_ERROR;
        }
        if (out) *out = it->second.i64;
        return AFROS_SUCCESS;
    }

    int32_t GetString(const std::string &key, std::string *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = map_.find(key);
        if (it == map_.end() || it->second.type != ParamType::STRING) {
            return AFROS_ERROR;
        }
        if (out) *out = it->second.s;
        return AFROS_SUCCESS;
    }

    int32_t GetBool(const std::string &key, bool *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = map_.find(key);
        if (it == map_.end() || it->second.type != ParamType::BOOL) {
            return AFROS_ERROR;
        }
        if (out) *out = it->second.b;
        return AFROS_SUCCESS;
    }

    int32_t GetBytes(const std::string &key, std::vector<uint8_t> *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = map_.find(key);
        if (it == map_.end() || it->second.type != ParamType::BYTES) {
            return AFROS_ERROR;
        }
        if (out) *out = it->second.bytes;
        return AFROS_SUCCESS;
    }

    bool HasKey(const std::string &key) const
    {
        std::lock_guard<std::mutex> g(lock_);
        return map_.find(key) != map_.end();
    }

    void Remove(const std::string &key)
    {
        std::lock_guard<std::mutex> g(lock_);
        map_.erase(key);
    }

    /** Number of keys currently stored. */
    size_t Size() const
    {
        std::lock_guard<std::mutex> g(lock_);
        return map_.size();
    }

    /** Snapshot all keys. */
    std::vector<std::string> Keys() const
    {
        std::lock_guard<std::mutex> g(lock_);
        std::vector<std::string> out;
        out.reserve(map_.size());
        for (const auto &kv : map_) {
            out.push_back(kv.first);
        }
        return out;
    }

private:
    mutable std::unordered_map<std::string, ParamValue> map_;
    mutable std::mutex lock_;
};

} // namespace afros_harmony

/* ---- C ABI ---- */

extern "C" {

using afros_harmony::WantParams;

WantParams *WantParamsNew(void)                          { return new WantParams(); }
void        WantParamsDelete(WantParams *p)              { delete p; }
int32_t     WantParamsSetInt32(WantParams *p, const char *k, int32_t v)
{ return p && k ? p->SetInt32(k, v) : AFROS_ERROR_INVALID_PARAM; }
int32_t     WantParamsSetString(WantParams *p, const char *k, const char *v)
{ return p && k && v ? p->SetString(k, v) : AFROS_ERROR_INVALID_PARAM; }
int32_t     WantParamsSetBool(WantParams *p, const char *k, bool v)
{ return p && k ? p->SetBool(k, v) : AFROS_ERROR_INVALID_PARAM; }
int32_t     WantParamsGetInt32(WantParams *p, const char *k, int32_t *out)
{ return p && k ? p->GetInt32(k, out) : AFROS_ERROR_INVALID_PARAM; }
size_t      WantParamsSize(WantParams *p)                { return p ? p->Size() : 0; }

} // extern "C"
