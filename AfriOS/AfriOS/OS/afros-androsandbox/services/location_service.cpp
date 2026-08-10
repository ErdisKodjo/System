/*
 * services/location_service.cpp — LocationManager.
 *
 * The LocationManager exposes location providers (GPS, NETWORK, PASSIVE)
 * to apps. Clients query the last known location, register for periodic
 * updates with a min-time / min-distance filter, and receive Location
 * objects via a listener callback.
 *
 * In the sandbox the GPS provider is synthesised: every minute the
 * manager publishes a new "fix" at a default location (the African
 * Union HQ, 9.0°N 38.7°E). Apps that register for updates receive a
 * stream of these fixes; real GPS hardware would replace the synthesiser.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>

#define LOC_PROVIDER_GPS     "gps"
#define LOC_PROVIDER_NETWORK "network"
#define LOC_PROVIDER_PASSIVE "passive"

struct Location {
    double  latitude;
    double  longitude;
    double  altitude;
    float   accuracy;
    float   speed;
    float   bearing;
    int64_t timestamp_ns;
    const char *provider;
};

struct LocationListener {
    int   id;
    std::string provider;
    int64_t min_time_ns;
    float   min_distance_m;
    Location last_delivered;
    void (*callback)(void *cookie, const Location *loc);
    void *cookie;
    bool  enabled;
};

class LocationManager {
public:
    LocationManager() : next_id_(1), running_(false) {
        /* Seed the last-known location for each provider. */
        for (const char *p : {LOC_PROVIDER_GPS, LOC_PROVIDER_NETWORK,
                              LOC_PROVIDER_PASSIVE}) {
            Location l{};
            l.latitude = 9.0;
            l.longitude = 38.7;
            l.altitude = 2350.0;
            l.accuracy = 5.0f;
            l.speed = 0.0f;
            l.bearing = 0.0f;
            l.timestamp_ns = NowNs();
            l.provider = p;
            last_known_[p] = l;
        }
    }
    ~LocationManager() { StopGpsThread(); }

    /* Returns 1 if the provider is available, 0 otherwise. */
    int IsProviderEnabled(const char *provider) {
        if (!provider) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        return last_known_.find(provider) != last_known_.end() ? 1 : 0;
    }

    /* Get the last known location for a provider; returns NAME_NOT_FOUND
     * if the provider doesn't exist. */
    status_t GetLastKnownLocation(const char *provider, Location *out) {
        if (!provider || !out) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        auto it = last_known_.find(provider);
        if (it == last_known_.end()) return NAME_NOT_FOUND;
        *out = it->second;
        return OK;
    }

    /* Register a listener; returns id >0 on success. */
    int RequestUpdates(const char *provider, int64_t min_time_ns,
                       float min_distance_m,
                       void (*cb)(void *, const Location *), void *cookie) {
        if (!provider || !cb) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        if (last_known_.find(provider) == last_known_.end()) return NAME_NOT_FOUND;
        LocationListener l;
        l.id = next_id_++;
        l.provider = provider;
        l.min_time_ns = min_time_ns > 0 ? min_time_ns : (int64_t)60'000'000'000LL;
        l.min_distance_m = min_distance_m;
        l.callback = cb;
        l.cookie = cookie;
        l.enabled = true;
        l.last_delivered = last_known_[provider];
        listeners_.push_back(std::move(l));
        StartGpsThreadLocked();
        return listeners_.back().id;
    }

    status_t RemoveUpdates(int id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
            if (it->id == id) { listeners_.erase(it); return OK; }
        }
        return NAME_NOT_FOUND;
    }

    /* List all available providers into `out`; returns count. */
    size_t GetAllProviders(const char **out, size_t max) {
        std::lock_guard<std::mutex> lk(mu_);
        size_t n = 0;
        for (auto &kv : last_known_) {
            if (n >= max) break;
            out[n++] = kv.first.c_str();
        }
        return n;
    }

private:
    static int64_t NowNs() {
        return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void StartGpsThreadLocked() {
        if (running_.exchange(true)) return;
        gps_thread_ = std::thread([this] {
            while (running_.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::lock_guard<std::mutex> lk(mu_);
                int64_t now = NowNs();
                /* Synthesise a small drift around the default fix. */
                for (auto &kv : last_known_) {
                    kv.second.latitude  += 0.0001 * (std::sin((double)now / 1e9));
                    kv.second.longitude += 0.0001 * (std::cos((double)now / 1e9));
                    kv.second.timestamp_ns = now;
                }
                /* Deliver to listeners whose filter is satisfied. */
                for (auto &l : listeners_) {
                    if (!l.enabled) continue;
                    auto it = last_known_.find(l.provider);
                    if (it == last_known_.end()) continue;
                    if (now - l.last_delivered.timestamp_ns < l.min_time_ns) continue;
                    Location loc = it->second;
                    double dlat = loc.latitude  - l.last_delivered.latitude;
                    double dlon = loc.longitude - l.last_delivered.longitude;
                    double dist = std::sqrt(dlat * dlat + dlon * dlon) * 111000.0;
                    if (dist < l.min_distance_m) continue;
                    l.last_delivered = loc;
                    l.callback(l.cookie, &loc);
                }
            }
        });
    }
    void StopGpsThread() {
        running_ = false;
        if (gps_thread_.joinable()) gps_thread_.join();
    }

    std::mutex mu_;
    std::unordered_map<std::string, Location> last_known_;
    std::vector<LocationListener> listeners_;
    int next_id_;
    std::atomic<bool> running_;
    std::thread gps_thread_;
};

static LocationManager *g_lm = nullptr;
static LocationManager *lm() {
    if (!g_lm) g_lm = new LocationManager();
    return g_lm;
}

extern "C" {

int  LocationIsProviderEnabled(const char *p) { return lm()->IsProviderEnabled(p); }
int  LocationGetLastKnown(const char *p, Location *out) {
    return lm()->GetLastKnownLocation(p, out);
}
int  LocationRequestUpdates(const char *p, long long min_ns, float min_m,
                            void (*cb)(void *, const Location *), void *cookie) {
    return lm()->RequestUpdates(p, (int64_t)min_ns, min_m, cb, cookie);
}
int  LocationRemoveUpdates(int id)            { return lm()->RemoveUpdates(id); }
size_t LocationGetAllProviders(const char **out, size_t max) {
    return lm()->GetAllProviders(out, max);
}

} /* extern "C" */
