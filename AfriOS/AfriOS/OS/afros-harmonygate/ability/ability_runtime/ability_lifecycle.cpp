/**
 * @file ability_lifecycle.cpp
 * @brief AfriOS HarmonyOS compatibility — Ability lifecycle callbacks.
 *
 * Mirrors the HarmonyOS Ability lifecycle state machine:
 *   UNINITIALIZED → INITIAL → INACTIVE → ACTIVE → INACTIVE → BACKGROUND
 *                  → FOREGROUND → INACTIVE → ACTIVE … → BACKGROUND → STOP.
 *
 * AbilityManager drives the transitions; this file owns the per-Ability
 * state object and the callback dispatch.
 */

#include "afros_harmony.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <iostream>

namespace afros_harmony {

/** Lifecycle states (HarmonyOS naming). */
enum class LifecycleState : uint32_t {
    UNINITIALIZED = 0,
    INITIAL,
    INACTIVE,
    ACTIVE,
    BACKGROUND,
    STOPPED,
};

/** Human-readable state name (diagnostics). */
static const char *state_name(LifecycleState s)
{
    switch (s) {
    case LifecycleState::UNINITIALIZED: return "UNINITIALIZED";
    case LifecycleState::INITIAL:       return "INITIAL";
    case LifecycleState::INACTIVE:      return "INACTIVE";
    case LifecycleState::ACTIVE:        return "ACTIVE";
    case LifecycleState::BACKGROUND:    return "BACKGROUND";
    case LifecycleState::STOPPED:       return "STOPPED";
    }
    return "?";
}

/** Per-Ability callback table. */
struct LifecycleCallbacks {
    void (*on_start)(void *user);
    void (*on_active)(void *user);
    void (*on_inactive)(void *user);
    void (*on_background)(void *user);
    void (*on_foreground)(void *user);
    void (*on_stop)(void *user);
};

/** Tracks the lifecycle of a single Ability instance. */
class AbilityLifecycle {
public:
    AbilityLifecycle() = default;

    /** Register the user-supplied callback table. */
    void SetCallbacks(const LifecycleCallbacks &cbs, void *user)
    {
        std::lock_guard<std::mutex> g(lock_);
        cbs_   = cbs;
        user_  = user;
    }

    /** Transition to INITIAL and fire onStart. */
    int32_t Start()
    {
        std::lock_guard<std::mutex> g(lock_);
        if (state_ != LifecycleState::UNINITIALIZED &&
            state_ != LifecycleState::STOPPED) {
            return AFROS_ERROR;
        }
        state_ = LifecycleState::INITIAL;
        if (cbs_.on_start) cbs_.on_start(user_);
        return AFROS_SUCCESS;
    }

    /** INITIAL/BACKGROUND → INACTIVE, then INACTIVE → ACTIVE. */
    int32_t Active()
    {
        std::lock_guard<std::mutex> g(lock_);
        if (state_ == LifecycleState::ACTIVE) {
            return AFROS_SUCCESS;
        }
        if (state_ == LifecycleState::INITIAL ||
            state_ == LifecycleState::BACKGROUND) {
            state_ = LifecycleState::INACTIVE;
            if (cbs_.on_inactive) cbs_.on_inactive(user_);
        }
        state_ = LifecycleState::ACTIVE;
        if (cbs_.on_active) cbs_.on_active(user_);
        return AFROS_SUCCESS;
    }

    /** ACTIVE → INACTIVE. */
    int32_t Inactive()
    {
        std::lock_guard<std::mutex> g(lock_);
        if (state_ != LifecycleState::ACTIVE) {
            return AFROS_ERROR;
        }
        state_ = LifecycleState::INACTIVE;
        if (cbs_.on_inactive) cbs_.on_inactive(user_);
        return AFROS_SUCCESS;
    }

    /** INACTIVE → BACKGROUND. */
    int32_t Background()
    {
        std::lock_guard<std::mutex> g(lock_);
        if (state_ != LifecycleState::INACTIVE &&
            state_ != LifecycleState::ACTIVE) {
            return AFROS_ERROR;
        }
        if (state_ == LifecycleState::ACTIVE && cbs_.on_inactive) {
            cbs_.on_inactive(user_);
        }
        state_ = LifecycleState::BACKGROUND;
        if (cbs_.on_background) cbs_.on_background(user_);
        return AFROS_SUCCESS;
    }

    /** BACKGROUND → INACTIVE → ACTIVE. */
    int32_t Foreground()
    {
        std::lock_guard<std::mutex> g(lock_);
        if (state_ != LifecycleState::BACKGROUND) {
            return AFROS_ERROR;
        }
        state_ = LifecycleState::INACTIVE;
        if (cbs_.on_foreground) cbs_.on_foreground(user_);
        state_ = LifecycleState::ACTIVE;
        if (cbs_.on_active) cbs_.on_active(user_);
        return AFROS_SUCCESS;
    }

    /** Any → STOPPED. */
    int32_t Stop()
    {
        std::lock_guard<std::mutex> g(lock_);
        if (state_ == LifecycleState::STOPPED) {
            return AFROS_SUCCESS;
        }
        if (state_ == LifecycleState::ACTIVE && cbs_.on_inactive) {
            cbs_.on_inactive(user_);
        }
        state_ = LifecycleState::STOPPED;
        if (cbs_.on_stop) cbs_.on_stop(user_);
        return AFROS_SUCCESS;
    }

    LifecycleState state() const { return state_; }
    const char *state_string() const { return state_name(state_); }

private:
    LifecycleState    state_ = LifecycleState::UNINITIALIZED;
    LifecycleCallbacks cbs_  = {};
    void             *user_  = nullptr;
    std::mutex        lock_;
};

} // namespace afros_harmony

/* ---- C ABI for HarmonyOS callers ---- */

extern "C" {

using afros_harmony::AbilityLifecycle;
using afros_harmony::LifecycleCallbacks;

/** Allocate a new lifecycle tracker. */
AbilityLifecycle *AbilityLifecycleNew(void)
{
    return new AbilityLifecycle();
}

void AbilityLifecycleDelete(AbilityLifecycle *lc)
{
    delete lc;
}

int32_t AbilityLifecycleSetCallbacks(AbilityLifecycle *lc,
                                     const LifecycleCallbacks *cbs,
                                     void *user)
{
    if (lc == nullptr || cbs == nullptr) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    lc->SetCallbacks(*cbs, user);
    return AFROS_SUCCESS;
}

int32_t AbilityLifecycleStart(AbilityLifecycle *lc)        { return lc ? lc->Start()      : AFROS_ERROR_INVALID_PARAM; }
int32_t AbilityLifecycleActive(AbilityLifecycle *lc)       { return lc ? lc->Active()     : AFROS_ERROR_INVALID_PARAM; }
int32_t AbilityLifecycleInactive(AbilityLifecycle *lc)     { return lc ? lc->Inactive()   : AFROS_ERROR_INVALID_PARAM; }
int32_t AbilityLifecycleBackground(AbilityLifecycle *lc)   { return lc ? lc->Background() : AFROS_ERROR_INVALID_PARAM; }
int32_t AbilityLifecycleForeground(AbilityLifecycle *lc)   { return lc ? lc->Foreground() : AFROS_ERROR_INVALID_PARAM; }
int32_t AbilityLifecycleStop(AbilityLifecycle *lc)         { return lc ? lc->Stop()       : AFROS_ERROR_INVALID_PARAM; }

const char *AbilityLifecycleStateString(AbilityLifecycle *lc)
{
    return lc ? lc->state_string() : "INVALID";
}

} // extern "C"
