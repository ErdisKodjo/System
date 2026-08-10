/**
 * @file want.cpp
 * @brief AfriOS HarmonyOS compatibility — Want description object.
 *
 * A Want describes an operation to perform: action (e.g. "ohos.want.action.view"),
 * entity (e.g. "entity.system.home"), uri, type, and a WantParams bundle of
 * extra parameters. Wants are passed to AbilityContext::StartAbility().
 *
 * This file is self-contained: it carries a minimal WantParams definition
 * rather than depending on want_params.cpp being part of the same TU.
 */

#include "afros_harmony.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <sstream>

namespace afros_harmony {

/** Minimal parameter bundle (kept here so want.cpp compiles standalone). */
class WantParams {
public:
    void SetString(const std::string &k, const std::string &v)
    { std::lock_guard<std::mutex> g(lock_); str_[k] = v; }
    int32_t GetString(const std::string &k, std::string *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = str_.find(k);
        if (it == str_.end()) return AFROS_ERROR;
        if (out) *out = it->second;
        return AFROS_SUCCESS;
    }
    void SetInt(const std::string &k, int32_t v)
    { std::lock_guard<std::mutex> g(lock_); ints_[k] = v; }
    int32_t GetInt(const std::string &k, int32_t *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = ints_.find(k);
        if (it == ints_.end()) return AFROS_ERROR;
        if (out) *out = it->second;
        return AFROS_SUCCESS;
    }
    size_t Size() const
    {
        std::lock_guard<std::mutex> g(lock_);
        return str_.size() + ints_.size();
    }
private:
    mutable std::unordered_map<std::string, std::string> str_;
    mutable std::unordered_map<std::string, int32_t>     ints_;
    mutable std::mutex lock_;
};

/** Bit flags affecting Want resolution. */
enum WantFlag : uint32_t {
    FLAG_NONE              = 0u,
    FLAG_READ_PERMISSION   = 1u << 0,
    FLAG_WRITE_PERMISSION  = 1u << 1,
    FLAG_NOT_OHOS_COMPONENT= 1u << 2,
    FLAG_ABILITYSLICE_FORWARD = 1u << 3,
};

/** Operation description. */
class Want {
public:
    Want() = default;

    void SetAction(const std::string &a)  { action_ = a; }
    const std::string &GetAction() const  { return action_; }

    void SetEntity(const std::string &e)  { entity_ = e; }
    const std::string &GetEntity() const  { return entity_; }

    void SetUri(const std::string &u)     { uri_ = u; }
    const std::string &GetUri() const     { return uri_; }

    void SetType(const std::string &t)    { type_ = t; }
    const std::string &GetType() const    { return type_; }

    void SetBundle(const std::string &b)  { bundle_ = b; }
    const std::string &GetBundle() const  { return bundle_; }

    void SetAbility(const std::string &a) { ability_ = a; }
    const std::string &GetAbility() const { return ability_; }

    void AddFlag(uint32_t f)              { flags_ |= f; }
    void ClearFlag(uint32_t f)            { flags_ &= ~f; }
    uint32_t GetFlags() const             { return flags_; }

    WantParams &Params()                  { return params_; }
    const WantParams &Params() const      { return params_; }

    /** Serialise into a simple key=value\n text format. */
    std::string Serialize() const
    {
        std::ostringstream os;
        os << "action=" << action_ << "\n"
           << "entity=" << entity_ << "\n"
           << "uri="    << uri_    << "\n"
           << "type="   << type_   << "\n"
           << "bundle=" << bundle_ << "\n"
           << "ability="<< ability_<< "\n"
           << "flags="  << flags_  << "\n";
        return os.str();
    }

    /** Deserialise from the format produced by Serialize(). */
    int32_t Deserialize(const std::string &s)
    {
        std::istringstream is(s);
        std::string line;
        while (std::getline(is, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            if      (k == "action")  action_  = v;
            else if (k == "entity")  entity_  = v;
            else if (k == "uri")     uri_     = v;
            else if (k == "type")    type_    = v;
            else if (k == "bundle")  bundle_  = v;
            else if (k == "ability") ability_ = v;
            else if (k == "flags")   flags_   = (uint32_t)std::stoul(v);
        }
        return AFROS_SUCCESS;
    }

private:
    std::string action_;
    std::string entity_;
    std::string uri_;
    std::string type_;
    std::string bundle_;
    std::string ability_;
    uint32_t    flags_ = 0;
    WantParams  params_;
};

} // namespace afros_harmony

/* ---- C ABI ---- */

extern "C" {

using afros_harmony::Want;

Want   *WantNew(void)                                  { return new Want(); }
void    WantDelete(Want *w)                            { delete w; }
void    WantSetAction(Want *w, const char *v)          { if (w && v) w->SetAction(v); }
void    WantSetEntity(Want *w, const char *v)          { if (w && v) w->SetEntity(v); }
void    WantSetUri(Want *w, const char *v)             { if (w && v) w->SetUri(v); }
void    WantSetBundle(Want *w, const char *v)          { if (w && v) w->SetBundle(v); }
void    WantSetAbility(Want *w, const char *v)         { if (w && v) w->SetAbility(v); }
void    WantAddFlag(Want *w, uint32_t f)               { if (w) w->AddFlag(f); }
uint32_t WantGetFlags(Want *w)                         { return w ? w->GetFlags() : 0; }
const char *WantGetAction(Want *w)                     { return w ? w->GetAction().c_str() : nullptr; }

} // extern "C"
