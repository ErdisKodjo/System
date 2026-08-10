/*
 * framework/telephony/telephony_manager.cpp — Telephony service.
 *
 * TelephonyManager is the client-facing API for the telephony service.
 * It exposes device- and subscription-level information: SIM state and
 * operator, network type (LTE, NR, etc.), signal strength, and the
 * active data subscription id. The implementation here is a stub backed
 * by static defaults; real Android would delegate to the phone process
 * via a binder interface.
 *
 * The stub picks reasonable defaults for a development device: SIM
 * present, operator "AfriOS Wireless", LTE network, signal -90 dBm.
 * Callers can override the values for testing.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <mutex>

/* SIM state constants — mirror android.telephony.TelephonyManager. */
enum SimState {
    SIM_STATE_UNKNOWN   = 0,
    SIM_STATE_ABSENT    = 1,
    SIM_STATE_PIN       = 2,
    SIM_STATE_PUK       = 3,
    SIM_STATE_READY     = 5,
    SIM_STATE_NOT_READY = 6,
};

enum NetworkType {
    NETWORK_UNKNOWN = 0,
    NETWORK_GPRS    = 1,
    NETWORK_EDGE    = 2,
    NETWORK_UMTS    = 3,
    NETWORK_HSDPA   = 8,
    NETWORK_HSUPA   = 9,
    NETWORK_LTE     = 13,
    NETWORK_NR      = 20,
};

struct TelephonyState {
    std::mutex mu;
    int   sim_state = SIM_STATE_READY;
    int   network_type = NETWORK_LTE;
    int   signal_dbm = -90;
    int   signal_level = 3;     /* 0..4 */
    int   default_sub_id = 1;
    std::string sim_operator = "AfriOS Wireless";
    std::string sim_operator_name = "AfriOS";
    std::string sim_country_iso = "af";
    std::string network_country_iso = "af";
    std::string sim_serial = "8901234567890123456";
    std::string subscriber_id = "310995000000001";
    std::string line1_number = "+93000000000";
    bool  airplane_mode = false;
    bool  data_enabled = true;
};

static TelephonyState *g_state = nullptr;
static TelephonyState *state() {
    if (!g_state) g_state = new TelephonyState();
    return g_state;
}

extern "C" {

int TelephonyGetSimState() {
    return state()->sim_state;
}
const char *TelephonyGetSimOperator() {
    static thread_local std::string s;
    s = state()->sim_operator;
    return s.c_str();
}
const char *TelephonyGetSimOperatorName() {
    static thread_local std::string s;
    s = state()->sim_operator_name;
    return s.c_str();
}
const char *TelephonyGetSimCountryIso() {
    static thread_local std::string s;
    s = state()->sim_country_iso;
    return s.c_str();
}
const char *TelephonyGetSimSerial() {
    static thread_local std::string s;
    s = state()->sim_serial;
    return s.c_str();
}
int  TelephonyGetNetworkType() {
    return state()->network_type;
}
int  TelephonyGetSignalDbm() {
    return state()->signal_dbm;
}
int  TelephonyGetSignalLevel() {
    return state()->signal_level;
}
const char *TelephonyGetNetworkCountryIso() {
    static thread_local std::string s;
    s = state()->network_country_iso;
    return s.c_str();
}
const char *TelephonyGetLine1Number() {
    static thread_local std::string s;
    s = state()->line1_number;
    return s.c_str();
}
int  TelephonyGetDataEnabled() { return state()->data_enabled ? 1 : 0; }
int  TelephonyGetAirplaneMode() { return state()->airplane_mode ? 1 : 0; }
int  TelephonyGetDefaultSubId() { return state()->default_sub_id; }

/* Test-only setters so callers can simulate changes. */
void TelephonySetSimState(int s)         { state()->sim_state = s; }
void TelephonySetNetworkType(int n)      { state()->network_type = n; }
void TelephonySetSignal(int dbm, int lvl){ state()->signal_dbm = dbm; state()->signal_level = lvl; }
void TelephonySetAirplaneMode(int on)    { state()->airplane_mode = on != 0; }
void TelephonySetDataEnabled(int on)     { state()->data_enabled = on != 0; }
void TelephonySetSimOperator(const char *s) {
    if (s) state()->sim_operator = s;
}

} /* extern "C" */
