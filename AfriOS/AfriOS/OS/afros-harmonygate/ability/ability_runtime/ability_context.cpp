/**
 * @file ability_context.cpp
 * @brief AfriOS HarmonyOS compatibility — AbilityContext.
 *
 * Each Ability instance owns an AbilityContext which provides access to
 * the local bundle's resources, the AbilityManager, and the ability to
 * start other abilities. This file mirrors the OHOS AbilityContext C++ API.
 */

#include "afros_harmony.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <mutex>
#include <iostream>

namespace afros_harmony {

/** Minimal forward declaration of the Want type used by StartAbility. */
struct WantRef {
    std::string action;
    std::string bundle;
    std::string ability;
};

/** Resource descriptor for a bundle asset. */
struct Resource {
    std::string path;       /**< Absolute on-disk path. */
    std::string media_type; /**< e.g. "image/png". */
    uint32_t    size_bytes = 0;
};

/** Per-Ability context. */
class AbilityContext {
public:
    AbilityContext() = default;
    explicit AbilityContext(const std::string &bundle,
                            const std::string &ability)
        : bundle_(bundle), ability_(ability) {}

    /** Set the bundle this context is bound to. */
    void SetBundle(const std::string &b)  { bundle_ = b; }
    const std::string &GetBundle() const  { return bundle_; }

    void SetAbility(const std::string &a) { ability_ = a; }
    const std::string &GetAbility() const { return ability_; }

    /** Register a resource so the ability can resolve it later. */
    int32_t RegisterResource(const std::string &name,
                             const Resource &res)
    {
        std::lock_guard<std::mutex> g(lock_);
        if (resources_.find(name) != resources_.end()) {
            return AFROS_ERROR;
        }
        resources_[name] = res;
        return AFROS_SUCCESS;
    }

    /** Look up a previously-registered resource. */
    int32_t GetResource(const std::string &name, Resource *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = resources_.find(name);
        if (it == resources_.end()) {
            return AFROS_ERROR;
        }
        if (out) *out = it->second;
        return AFROS_SUCCESS;
    }

    /**
     * @brief Start another ability identified by the supplied Want.
     *        Delegates to the global AbilityManager (forward-declared).
     * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
     */
    int32_t StartAbility(const WantRef &want)
    {
        if (want.bundle.empty() && want.ability.empty() &&
            want.action.empty()) {
            return AFROS_ERROR_INVALID_PARAM;
        }
        /* In the sandbox we just log the request; a real implementation
         * would forward to AbilityManager::StartAbility(want). */
        std::cout << "[AbilityContext] StartAbility bundle=" << want.bundle
                  << " ability=" << want.ability
                  << " action="   << want.action << std::endl;
        return AFROS_SUCCESS;
    }

    /** Terminate the current ability (no-op in the sandbox). */
    int32_t TerminateAbility()
    {
        std::cout << "[AbilityContext] TerminateAbility "
                  << bundle_ << "/" << ability_ << std::endl;
        return AFROS_SUCCESS;
    }

    /** Min-helpers for string-based access. */
    int32_t StartAbilityByName(const std::string &bundle,
                               const std::string &ability)
    {
        WantRef w;
        w.bundle  = bundle;
        w.ability = ability;
        return StartAbility(w);
    }

    size_t ResourceCount() const
    {
        std::lock_guard<std::mutex> g(lock_);
        return resources_.size();
    }

private:
    std::string bundle_;
    std::string ability_;
    mutable std::mutex lock_;
    std::unordered_map<std::string, Resource> resources_;
};

} // namespace afros_harmony

/* ---- C ABI ---- */

extern "C" {

using afros_harmony::AbilityContext;
using afros_harmony::Resource;
using afros_harmony::WantRef;

AbilityContext *AbilityContextNew(const char *bundle, const char *ability)
{
    auto *c = new AbilityContext();
    if (bundle)  c->SetBundle(bundle);
    if (ability) c->SetAbility(ability);
    return c;
}

void AbilityContextDelete(AbilityContext *c) { delete c; }

int32_t AbilityContextRegisterResource(AbilityContext *c,
                                       const char *name,
                                       const char *path,
                                       const char *media_type,
                                       uint32_t    size_bytes)
{
    if (c == nullptr || name == nullptr || path == nullptr) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    Resource r;
    r.path        = path;
    r.media_type  = media_type ? media_type : "";
    r.size_bytes  = size_bytes;
    return c->RegisterResource(name, r);
}

int32_t AbilityContextStartAbility(AbilityContext *c,
                                   const char *bundle,
                                   const char *ability,
                                   const char *action)
{
    if (c == nullptr) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    WantRef w;
    if (bundle)  w.bundle  = bundle;
    if (ability) w.ability = ability;
    if (action)  w.action  = action;
    return c->StartAbility(w);
}

int32_t AbilityContextTerminate(AbilityContext *c)
{
    return c ? c->TerminateAbility() : AFROS_ERROR_INVALID_PARAM;
}

size_t AbilityContextResourceCount(AbilityContext *c)
{
    return c ? c->ResourceCount() : 0;
}

} // extern "C"
