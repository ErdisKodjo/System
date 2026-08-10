/**
 * @file ability_manager.cpp
 * @brief AfriOS HarmonyOS compatibility — AbilityManager.
 *
 * Loads abilities from HarmonyOS Ability Packages (.hap), manages the
 * ability stack (LIFO of foreground/background slices), and drives their
 * lifecycle transitions via AbilityLifecycle.
 *
 * The sandbox emulates a HAP as a directory containing a stub config.json;
 * on real hardware this would unzip the .hap, validate the signature and
 * hand the ability off to the ACE runtime for rendering.
 */

#include "afros_harmony.h"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>

namespace afros_harmony {

/** Minimal lifecycle state enum (mirrors ability_lifecycle.cpp). */
enum class LifecycleState : uint32_t {
    UNINITIALIZED = 0, INITIAL, INACTIVE, ACTIVE, BACKGROUND, STOPPED,
};

/** A loaded ability record. */
struct AbilityRecord {
    std::string    bundle;
    std::string    ability;
    std::string    hap_path;
    LifecycleState state      = LifecycleState::UNINITIALIZED;
    bool           is_main    = false;
    uint64_t       started_ms = 0;
};

/**
 * AbilityManager: owns the ability stack and the HAP→bundle registry.
 */
class AbilityManager {
public:
    static AbilityManager &Instance()
    {
        static AbilityManager inst;
        return inst;
    }

    /**
     * @brief Load (or reload) a HAP into the registry.
     * @param hap_path  Path to the .hap file or its unpacked directory.
     * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
     */
    int32_t LoadAbility(const std::string &hap_path)
    {
        if (hap_path.empty()) {
            return AFROS_ERROR_INVALID_PARAM;
        }
        std::string bundle  = derive_bundle_name(hap_path);
        std::string ability = "MainAbility";

        std::lock_guard<std::mutex> g(lock_);
        if (registry_.find(bundle) == registry_.end()) {
            AbilityRecord rec;
            rec.bundle    = bundle;
            rec.ability   = ability;
            rec.hap_path  = hap_path;
            rec.is_main   = true;
            registry_[bundle] = rec;
            std::cout << "[AbilityManager] Loaded " << bundle
                      << " from " << hap_path << std::endl;
        }
        return AFROS_SUCCESS;
    }

    /**
     * @brief Push an ability onto the active stack and run onStart/onActive.
     * @param bundle   Bundle name (must already be loaded).
     * @param ability  Ability name within the bundle.
     * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
     */
    int32_t StartAbility(const std::string &bundle,
                         const std::string &ability)
    {
        std::lock_guard<std::mutex> g(lock_);
        auto it = registry_.find(bundle);
        if (it == registry_.end()) {
            std::cerr << "[AbilityManager] Bundle not found: "
                      << bundle << std::endl;
            return AFROS_ERROR;
        }
        AbilityRecord rec = it->second;
        rec.ability      = ability.empty() ? it->second.ability : ability;
        rec.state        = LifecycleState::INITIAL;
        rec.started_ms   = now_ms();
        if (!stack_.empty()) {
            stack_.back().state = LifecycleState::BACKGROUND;
        }
        stack_.push_back(rec);
        stack_.back().state = LifecycleState::ACTIVE;
        std::cout << "[AbilityManager] Started " << bundle
                  << "/" << stack_.back().ability
                  << " (stack depth=" << stack_.size() << ")" << std::endl;
        return AFROS_SUCCESS;
    }

    /**
     * @brief Pop the top ability and resume the previous one.
     */
    int32_t TerminateAbility(const std::string &bundle)
    {
        std::lock_guard<std::mutex> g(lock_);
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
            if (it->bundle == bundle) {
                std::cout << "[AbilityManager] Terminating "
                          << bundle << "/" << it->ability << std::endl;
                /* erase() needs a forward iterator. */
                auto fwd = std::next(it).base();
                stack_.erase(fwd);
                if (!stack_.empty()) {
                    stack_.back().state = LifecycleState::ACTIVE;
                }
                return AFROS_SUCCESS;
            }
        }
        return AFROS_ERROR;
    }

    /** @brief Get the top-of-stack ability. */
    int32_t GetTopAbility(AbilityRecord *out) const
    {
        std::lock_guard<std::mutex> g(lock_);
        if (stack_.empty()) {
            return AFROS_ERROR;
        }
        if (out) *out = stack_.back();
        return AFROS_SUCCESS;
    }

    size_t StackDepth() const
    {
        std::lock_guard<std::mutex> g(lock_);
        return stack_.size();
    }

    size_t RegistrySize() const
    {
        std::lock_guard<std::mutex> g(lock_);
        return registry_.size();
    }

private:
    AbilityManager() = default;

    /** Derive a synthetic bundle name from the HAP path. */
    static std::string derive_bundle_name(const std::string &hap_path)
    {
        size_t slash = hap_path.find_last_of('/');
        std::string base = (slash == std::string::npos)
                           ? hap_path
                           : hap_path.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos && base.substr(dot) == ".hap") {
            base = base.substr(0, dot);
        }
        return base.empty() ? "com.afros.unknown" : base;
    }

    static uint64_t now_ms()
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
    }
    /* timespec needs <time.h>. */
    /* forward declaration below avoids ordering issues in some toolchains. */

    mutable std::mutex lock_;
    std::vector<AbilityRecord>                       stack_;
    std::unordered_map<std::string, AbilityRecord>   registry_;
};

} // namespace afros_harmony

/* ---- C ABI ---- */

/**
 * @note harmony_init / harmony_launch_app are declared in afros_harmony.h
 *       with C++ linkage (no `extern "C"` wrapper); their definitions here
 *       must match that linkage so the compiler sees the same declaration.
 */
using afros_harmony::AbilityManager;
using afros_harmony::AbilityRecord;

/** Initialise the HarmonyOS compatibility layer. */
afros_status_t harmony_init(void)
{
    std::cout << "[HARMONY-GATE] Initializing HarmonyOS Ability Runtime..."
              << std::endl;
    std::cout << "[HARMONY-GATE] Setting up ACE engine compatibility layer..."
              << std::endl;
    /* Touch the singleton so its constructor runs eagerly. */
    (void)AbilityManager::Instance();
    return AFROS_SUCCESS;
}

/** Launch a HarmonyOS application from a HAP file. */
afros_status_t harmony_launch_app(const char *hap_path)
{
    if (hap_path == nullptr) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    std::cout << "[HARMONY-GATE] Lancement de l'application HarmonyOS (.hap) : "
              << hap_path << std::endl;
    AbilityManager &am = AbilityManager::Instance();
    if (am.LoadAbility(hap_path) != AFROS_SUCCESS) {
        std::cerr << "[HARMONY-GATE] Failed to load HAP" << std::endl;
        return AFROS_ERROR;
    }
    /* Extract the bundle name from the path the same way LoadAbility does. */
    std::string base;
    {
        size_t slash = std::string(hap_path).find_last_of('/');
        base = (slash == std::string::npos)
               ? std::string(hap_path)
               : std::string(hap_path).substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos && base.substr(dot) == ".hap") {
            base = base.substr(0, dot);
        }
    }
    if (am.StartAbility(base, "MainAbility") != AFROS_SUCCESS) {
        std::cerr << "[HARMONY-GATE] Failed to start MainAbility" << std::endl;
        return AFROS_ERROR;
    }
    std::cout << "[HARMONY-GATE] Succès : application HarmonyOS démarrée."
              << std::endl;
    return AFROS_SUCCESS;
}

extern "C" {

int32_t AbilityManagerLoadAbility(const char *hap_path)
{
    return AbilityManager::Instance().LoadAbility(hap_path ? hap_path : "");
}

int32_t AbilityManagerStartAbility(const char *bundle, const char *ability)
{
    return AbilityManager::Instance().StartAbility(
        bundle ? bundle : "", ability ? ability : "");
}

int32_t AbilityManagerTerminateAbility(const char *bundle)
{
    return AbilityManager::Instance().TerminateAbility(
        bundle ? bundle : "");
}

size_t AbilityManagerStackDepth(void)
{
    return AbilityManager::Instance().StackDepth();
}

} // extern "C"
