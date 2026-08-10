/*
 * framework/telephony/phone_interface.cpp — Phone interface stub.
 *
 * The Phone interface is the internal (binder) API that
 * TelephonyManager delegates telephony *actions* to: place a call,
 * answer an incoming call, end the active call, hold, swap, etc. The
 * implementation lives in the phone process (com.android.phone) and is
 * exposed to the rest of the system via an ITelephony AIDL.
 *
 * This module provides the call-state machine (IDLE → RINGING → OFFHOOK
 * → IDLE) and stubs for the action methods. The sandbox has no real
 * modem, so the state machine is the whole point: callers can verify
 * that the right transitions happen in response to action calls.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

enum CallState {
    CALL_IDLE     = 0,
    CALL_RINGING  = 1,
    CALL_OFFHOOK  = 2,
    CALL_DIALING  = 3,
    CALL_ACTIVE   = 4,
    CALL_HOLDING  = 5,
    CALL_DISCONNECTED = 6,
};

struct PhoneCall {
    int   state;
    std::string number;       /* remote phone number */
    int   id;
    bool  incoming;
    bool  muted;
    bool  speaker;
    int   start_time_ms;      /* when the call became active, 0 if not */
};

class PhoneInterface {
public:
    PhoneInterface() : next_id_(1) {}

    status_t Call(const char *number) {
        if (!number || !number[0]) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state != CALL_IDLE) return INVALID_OPERATION;
        current_.state = CALL_DIALING;
        current_.number = number;
        current_.incoming = false;
        current_.id = next_id_++;
        current_.start_time_ms = 0;
        /* Auto-transition to ACTIVE (sandbox: the call connects instantly). */
        current_.state = CALL_ACTIVE;
        current_.start_time_ms = NowMs();
        return OK;
    }

    status_t Answer() {
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state != CALL_RINGING) return INVALID_OPERATION;
        current_.state = CALL_OFFHOOK;
        current_.state = CALL_ACTIVE;
        current_.start_time_ms = NowMs();
        return OK;
    }

    status_t EndCall() {
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state == CALL_IDLE || current_.state == CALL_DISCONNECTED)
            return INVALID_OPERATION;
        current_.state = CALL_DISCONNECTED;
        /* Reset for the next call. */
        history_.push_back(current_);
        current_ = PhoneCall{};
        current_.state = CALL_IDLE;
        return OK;
    }

    status_t Hold() {
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state != CALL_ACTIVE) return INVALID_OPERATION;
        current_.state = CALL_HOLDING;
        return OK;
    }

    status_t Unhold() {
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state != CALL_HOLDING) return INVALID_OPERATION;
        current_.state = CALL_ACTIVE;
        return OK;
    }

    status_t SetMute(bool mute) {
        std::lock_guard<std::mutex> lk(mu_);
        current_.muted = mute;
        return OK;
    }

    status_t SetSpeaker(bool on) {
        std::lock_guard<std::mutex> lk(mu_);
        current_.speaker = on;
        return OK;
    }

    /* Simulate an incoming call. The next Answer() will accept it. */
    status_t SimulateIncoming(const char *number) {
        if (!number) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state != CALL_IDLE) return INVALID_OPERATION;
        current_.state = CALL_RINGING;
        current_.number = number;
        current_.incoming = true;
        current_.id = next_id_++;
        current_.start_time_ms = 0;
        return OK;
    }

    int GetCallState() {
        std::lock_guard<std::mutex> lk(mu_);
        return current_.state;
    }
    const char *GetCallNumber() {
        std::lock_guard<std::mutex> lk(mu_);
        cached_ = current_.number;
        return cached_.c_str();
    }
    int GetCallDurationMs() {
        std::lock_guard<std::mutex> lk(mu_);
        if (current_.state != CALL_ACTIVE && current_.state != CALL_HOLDING)
            return 0;
        return NowMs() - current_.start_time_ms;
    }

private:
    static int NowMs() {
        return (int)(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    std::mutex mu_;
    PhoneCall current_{};
    std::vector<PhoneCall> history_;
    int next_id_;
    std::string cached_;   /* for GetCallNumber() */
};

static PhoneInterface *g_phone = nullptr;
static PhoneInterface *phone() {
    if (!g_phone) g_phone = new PhoneInterface();
    return g_phone;
}

extern "C" {

int  PhoneCall(const char *number)      { return (int)phone()->Call(number); }
int  PhoneAnswerRingingCall()           { return (int)phone()->Answer(); }
int  PhoneEndCall()                     { return (int)phone()->EndCall(); }
int  PhoneHoldCall()                    { return (int)phone()->Hold(); }
int  PhoneUnholdCall()                  { return (int)phone()->Unhold(); }
void PhoneSetMute(int mute)             { phone()->SetMute(mute != 0); }
void PhoneSetSpeaker(int on)            { phone()->SetSpeaker(on != 0); }
int  PhoneSimulateIncoming(const char *n) { return (int)phone()->SimulateIncoming(n); }
int  PhoneGetCallState()                { return phone()->GetCallState(); }
const char *PhoneGetCallNumber()        { return phone()->GetCallNumber(); }
int  PhoneGetCallDurationMs()           { return phone()->GetCallDurationMs(); }

} /* extern "C" */
