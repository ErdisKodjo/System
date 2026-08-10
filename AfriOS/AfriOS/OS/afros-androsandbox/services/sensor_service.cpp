/*
 * services/sensor_service.cpp — SensorService.
 *
 * The SensorService exposes the device's sensors (accelerometer,
 * gyroscope, magnetometer, light, proximity, etc.) to apps. Clients
 * query the available sensor list, register listeners with a sampling
 * period, and receive sensor events on a per-listener queue.
 *
 * In the sandbox there is no real sensor hardware; the service still
 * maintains a real sensor list and a real listener table, and synthesises
 * plausible default values for each sensor so apps can test the API
 * without a device.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <chrono>
#include <thread>

enum SensorType {
    SENSOR_ACCEL       = 1,
    SENSOR_MAG         = 2,
    SENSOR_GYRO        = 4,
    SENSOR_LIGHT       = 5,
    SENSOR_PROXIMITY   = 8,
    SENSOR_GRAVITY     = 9,
    SENSOR_LINEAR_ACCEL= 10,
    SENSOR_HUMIDITY    = 12,
    SENSOR_PRESSURE    = 6,
};

struct SensorInfo {
    int   type;
    char  name[64];
    char  vendor[64];
    int   min_delay_us;
    float max_range;
    float resolution;
    bool  present;
};

struct SensorEvent {
    int   type;
    int64_t timestamp_ns;
    float values[6];
    int   value_count;
};

struct SensorListener {
    int   id;
    int   sensor_type;
    int   sampling_period_us;
    std::queue<SensorEvent> queue;
    bool  enabled;
};

class SensorService {
public:
    SensorService() : next_id_(1), sample_thread_running_(false) {
        std::lock_guard<std::mutex> lk(mu_);
        Register(SENSOR_ACCEL,        "Accelerometer",        "AfriOS", 5000, 19.6f, 0.0024f);
        Register(SENSOR_MAG,          "Magnetometer",         "AfriOS", 20000, 2000.0f, 0.1f);
        Register(SENSOR_GYRO,         "Gyroscope",            "AfriOS", 5000, 34.9f, 0.001f);
        Register(SENSOR_LIGHT,        "Light Sensor",         "AfriOS", 100000, 40000.0f, 1.0f);
        Register(SENSOR_PROXIMITY,    "Proximity Sensor",     "AfriOS", 100000, 5.0f, 5.0f);
        Register(SENSOR_GRAVITY,      "Gravity Sensor",       "AfriOS", 5000, 19.6f, 0.0024f);
        Register(SENSOR_LINEAR_ACCEL, "Linear Acceleration",  "AfriOS", 5000, 19.6f, 0.0024f);
        Register(SENSOR_PRESSURE,     "Barometer",            "AfriOS", 100000, 1100.0f, 0.005f);
    }

    ~SensorService() { StopSampleThread(); }

    int GetSensorCount() {
        std::lock_guard<std::mutex> lk(mu_);
        int n = 0; for (auto &s : sensors_) if (s.present) n++;
        return n;
    }

    int GetSensorList(SensorInfo *out, int max) {
        std::lock_guard<std::mutex> lk(mu_);
        int n = 0;
        for (auto &s : sensors_) {
            if (n >= max) break;
            if (s.present) out[n++] = s;
        }
        return n;
    }

    /* Register a listener for a sensor type; returns listener id >0. */
    int RegisterListener(int sensor_type, int period_us) {
        std::lock_guard<std::mutex> lk(mu_);
        bool found = false;
        for (auto &s : sensors_) if (s.type == sensor_type && s.present) found = true;
        if (!found) return NAME_NOT_FOUND;
        SensorListener l;
        l.id = next_id_++;
        l.sensor_type = sensor_type;
        l.sampling_period_us = period_us > 0 ? period_us : 5000;
        l.enabled = true;
        listeners_.push_back(std::move(l));
        StartSampleThreadLocked();
        return listeners_.back().id;
    }

    status_t UnregisterListener(int id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
            if (it->id == id) { listeners_.erase(it); return OK; }
        }
        return NAME_NOT_FOUND;
    }

    /* Pop the next event for `id`; returns WOULD_BLOCK if the queue is
     * empty. */
    status_t PollEvent(int id, SensorEvent *out) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &l : listeners_) {
            if (l.id == id) {
                if (l.queue.empty()) return WOULD_BLOCK;
                *out = l.queue.front();
                l.queue.pop();
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

private:
    void Register(int type, const char *name, const char *vendor,
                  int min_delay, float range, float res) {
        SensorInfo s{};
        s.type = type;
        std::strncpy(s.name, name, sizeof(s.name) - 1);
        std::strncpy(s.vendor, vendor, sizeof(s.vendor) - 1);
        s.min_delay_us = min_delay;
        s.max_range = range;
        s.resolution = res;
        s.present = true;
        sensors_.push_back(s);
    }

    void StartSampleThreadLocked() {
        if (sample_thread_running_.exchange(true)) return;
        sample_thread_ = std::thread([this] {
            while (sample_thread_running_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                std::lock_guard<std::mutex> lk(mu_);
                int64_t now = (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                for (auto &l : listeners_) {
                    if (!l.enabled) continue;
                    SensorEvent e{};
                    e.type = l.sensor_type;
                    e.timestamp_ns = now;
                    /* Plausible defaults per sensor type. */
                    switch (l.sensor_type) {
                    case SENSOR_ACCEL: case SENSOR_LINEAR_ACCEL:
                        e.values[0] = 0.0f; e.values[1] = 0.0f; e.values[2] = 9.81f;
                        e.value_count = 3; break;
                    case SENSOR_GYRO:
                        e.values[0] = e.values[1] = e.values[2] = 0.0f;
                        e.value_count = 3; break;
                    case SENSOR_MAG:
                        e.values[0] = 25.0f; e.values[1] = -5.0f; e.values[2] = 40.0f;
                        e.value_count = 3; break;
                    case SENSOR_LIGHT:
                        e.values[0] = 200.0f; e.value_count = 1; break;
                    case SENSOR_PROXIMITY:
                        e.values[0] = 5.0f; e.value_count = 1; break;
                    case SENSOR_PRESSURE:
                        e.values[0] = 1013.25f; e.value_count = 1; break;
                    default:
                        e.values[0] = 0.0f; e.value_count = 1; break;
                    }
                    if (l.queue.size() < 64) l.queue.push(e);
                }
            }
        });
    }
    void StopSampleThread() {
        sample_thread_running_ = false;
        if (sample_thread_.joinable()) sample_thread_.join();
    }

    std::mutex mu_;
    std::vector<SensorInfo> sensors_;
    std::vector<SensorListener> listeners_;
    int next_id_;
    std::atomic<bool> sample_thread_running_;
    std::thread sample_thread_;
};

static SensorService *g_ss = nullptr;
static SensorService *ss() {
    if (!g_ss) g_ss = new SensorService();
    return g_ss;
}

extern "C" {

int  SensorServiceGetCount() { return ss()->GetSensorCount(); }
int  SensorServiceGetList(SensorInfo *out, int max) { return ss()->GetSensorList(out, max); }
int  SensorServiceRegisterListener(int type, int us) { return ss()->RegisterListener(type, us); }
int  SensorServiceUnregisterListener(int id) { return ss()->UnregisterListener(id); }
int  SensorServicePollEvent(int id, SensorEvent *out) {
    return ss()->PollEvent(id, out);
}

} /* extern "C" */
