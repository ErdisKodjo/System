/**
 * @file sensor_sharing.c
 * @brief AfriOS HarmonyOS compatibility — distributed sensor sharing.
 *
 * Allows a remote device to subscribe to a local sensor's stream. The
 * local side samples the sensor at the requested period and pushes events
 * over a SoftBus stream. Multiple subscribers per sensor are supported.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define AFROS_SE_SH_MAX_SUBS     8
#define AFROS_SE_SH_MAX_SENSORS  16
#define AFROS_SE_SH_NAME_LEN     32

/** Sensor event payload. */
typedef struct {
    int64_t  timestamp_ns;
    int32_t  sensor_id;
    float    values[6];   /**< Up to 6 axes (accel/quat/...). */
    uint8_t  value_count;
} afros_se_event_t;

/** SoftBus stream send callback. */
typedef int32_t (*afros_se_send_fn)(int32_t sid, const uint8_t *buf, uint32_t len);

typedef struct {
    int32_t         sensor_id;
    int32_t         stream_id;
    afros_se_send_fn send_fn;
    uint64_t        period_ms;
    bool            active;
    bool            in_use;
    pthread_t       thread;
} afros_se_sub_t;

static afros_se_sub_t g_subs[AFROS_SE_SH_MAX_SUBS];

/** Static registry of local sensors we know how to synthesise. */
static const struct {
    int32_t id;
    char    name[AFROS_SE_SH_NAME_LEN];
    uint8_t axis_count;
} g_registry[AFROS_SE_SH_MAX_SENSORS] = {
    { 1, "accelerometer", 3 },
    { 2, "gyroscope",     3 },
    { 3, "magnetometer",  3 },
    { 4, "light",         1 },
    { 5, "proximity",     1 },
    { 6, "pressure",      1 },
    { 7, "humidity",      1 },
    { 8, "temperature",   1 },
};

static afros_se_sub_t *find_free_sub(void)
{
    for (uint32_t i = 0; i < AFROS_SE_SH_MAX_SUBS; ++i) {
        if (!g_subs[i].in_use) {
            memset(&g_subs[i], 0, sizeof(g_subs[i]));
            g_subs[i].in_use = true;
            return &g_subs[i];
        }
    }
    return NULL;
}

static afros_se_sub_t *get_sub(int32_t handle)
{
    if (handle < 0 || handle >= AFROS_SE_SH_MAX_SUBS) {
        return NULL;
    }
    if (!g_subs[handle].in_use) {
        return NULL;
    }
    return &g_subs[handle];
}

/** Sample a synthetic sensor event for the given sensor id. */
static void sample_sensor(int32_t sensor_id, afros_se_event_t *ev)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ev->timestamp_ns = (int64_t)ts.tv_sec * 1000000000L + ts.tv_nsec;
    ev->sensor_id    = sensor_id;
    switch (sensor_id) {
    case 1: /* accel */
        ev->values[0] = 0.0f; ev->values[1] = 0.0f; ev->values[2] = 9.81f;
        ev->value_count = 3; break;
    case 2: /* gyro */
        ev->values[0] = ev->values[1] = ev->values[2] = 0.0f;
        ev->value_count = 3; break;
    case 3: /* mag */
        ev->values[0] = 25.0f; ev->values[1] = -5.0f; ev->values[2] = 40.0f;
        ev->value_count = 3; break;
    case 4: ev->values[0] = 200.0f; ev->value_count = 1; break; /* light */
    case 5: ev->values[0] = 5.0f;   ev->value_count = 1; break; /* prox cm */
    case 6: ev->values[0] = 1013.25f; ev->value_count = 1; break; /* hPa */
    case 7: ev->values[0] = 45.0f;  ev->value_count = 1; break; /* %RH */
    case 8: ev->values[0] = 25.0f;  ev->value_count = 1; break; /* °C */
    default:
        ev->values[0] = 0.0f; ev->value_count = 0; break;
    }
}

/** Sampling thread: pushes one event per period. */
static void *sample_thread(void *arg)
{
    afros_se_sub_t *s = (afros_se_sub_t *)arg;
    while (s->active) {
        afros_se_event_t ev;
        sample_sensor(s->sensor_id, &ev);
        s->send_fn(s->stream_id, (const uint8_t *)&ev, sizeof(ev));
        struct timespec ts = { .tv_sec  = s->period_ms / 1000U,
                               .tv_nsec = (s->period_ms % 1000U) * 1000000L };
        nanosleep(&ts, NULL);
    }
    return NULL;
}

/**
 * @brief Subscribe a remote device to a local sensor.
 * @param sensor_id   Local sensor id (see registry above).
 * @param stream_id   SoftBus stream id.
 * @param send_fn     SoftBus stream send callback.
 * @param period_ms   Sampling period in ms (must be ≥ 1).
 * @return Subscription handle ≥ 0, or -AFROS_ERROR_* on failure.
 */
int32_t SensorShareSubscribe(int32_t sensor_id,
                             int32_t stream_id,
                             afros_se_send_fn send_fn,
                             uint64_t period_ms)
{
    if (send_fn == NULL || period_ms == 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    bool known = false;
    for (uint32_t i = 0;
         i < sizeof(g_registry) / sizeof(g_registry[0]); ++i) {
        if (g_registry[i].id == sensor_id) {
            known = true;
            break;
        }
    }
    if (!known) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_se_sub_t *s = find_free_sub();
    if (s == NULL) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    s->sensor_id = sensor_id;
    s->stream_id = stream_id;
    s->send_fn   = send_fn;
    s->period_ms = period_ms;
    s->active    = true;
    if (pthread_create(&s->thread, NULL, sample_thread, s) != 0) {
        s->in_use = false;
        return -AFROS_ERROR;
    }
    return (int32_t)(s - g_subs);
}

/**
 * @brief Cancel a sensor subscription.
 */
int32_t SensorShareUnsubscribe(int32_t handle)
{
    afros_se_sub_t *s = get_sub(handle);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    s->active = false;
    pthread_join(s->thread, NULL);
    memset(s, 0, sizeof(*s));
    return AFROS_SUCCESS;
}

/** @brief Receive the next sensor event on the receiver side. */
int32_t SensorShareRecvEvent(int32_t (*recv_fn)(int32_t, uint8_t *, uint32_t),
                             afros_se_event_t *out)
{
    if (recv_fn == NULL || out == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    int32_t n = recv_fn(0, (uint8_t *)out, sizeof(*out));
    if (n < (int32_t)sizeof(*out)) {
        return AFROS_ERROR_TIMEOUT;
    }
    return AFROS_SUCCESS;
}

/** @brief Enumerate local sensors available for sharing. */
int32_t SensorShareList(int32_t *out_ids, uint32_t cap, uint32_t *out_count)
{
    if (out_count == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    uint32_t total = sizeof(g_registry) / sizeof(g_registry[0]);
    uint32_t n = (cap < total) ? cap : total;
    for (uint32_t i = 0; i < n; ++i) {
        if (out_ids != NULL) {
            out_ids[i] = g_registry[i].id;
        }
    }
    *out_count = n;
    return AFROS_SUCCESS;
}

/** @brief Look up a sensor's name and axis count. */
int32_t SensorShareDescribe(int32_t sensor_id, char *name, uint32_t cap,
                            uint8_t *axis_count)
{
    for (uint32_t i = 0;
         i < sizeof(g_registry) / sizeof(g_registry[0]); ++i) {
        if (g_registry[i].id == sensor_id) {
            if (name != NULL && cap > 0) {
                strncpy(name, g_registry[i].name, cap - 1);
                name[cap - 1] = '\0';
            }
            if (axis_count != NULL) {
                *axis_count = g_registry[i].axis_count;
            }
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR;
}
