/**
 * @file device_monitor.c
 * @brief AfriOS HarmonyOS compatibility — device online/offline monitor.
 *
 * Wraps the device_discovery module and emits online/offline transitions
 * to registered listeners. Maintains the authoritative device list that
 * CapabilityManager / TrustManager consume.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define AFROS_MON_MAX_DEVICES  32
#define AFROS_MON_LISTENERS    8
#define AFROS_MON_OFFLINE_MS   5000  /**< Mark a device offline after 5 s silent. */
#define AFROS_MON_TICK_MS      1000

/** Online/offline event delivered to listeners. */
typedef enum {
    AFROS_DEV_EVENT_ONLINE  = 1,
    AFROS_DEV_EVENT_OFFLINE = 2,
} afros_dev_event_t;

/** Forward-declare the device record (definition is private to discovery). */
typedef struct {
    char      device_id[64];
    char      hostname[64];
    char      addr[64];
    uint16_t  port;
    uint64_t  first_seen_ms;
    uint64_t  last_seen_ms;
    bool      valid;
} afros_mon_device_t;

/** Listener callback. */
typedef void (*afros_dev_listener_t)(const char *device_id,
                                     afros_dev_event_t event,
                                     void *user_data);

typedef struct {
    afros_dev_listener_t cb;
    void                *user_data;
    bool                 used;
} afros_mon_listener_slot_t;

static struct {
    bool                      initialised;
    bool                      running;
    pthread_t                 thread;
    pthread_mutex_t           lock;
    afros_mon_device_t        devices[AFROS_MON_MAX_DEVICES];
    uint32_t                  count;
    afros_mon_listener_slot_t listeners[AFROS_MON_LISTENERS];
} g_mon = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

static void dispatch_event_locked(const char *id, afros_dev_event_t ev)
{
    for (uint32_t i = 0; i < AFROS_MON_LISTENERS; ++i) {
        if (g_mon.listeners[i].used && g_mon.listeners[i].cb) {
            g_mon.listeners[i].cb(id, ev, g_mon.listeners[i].user_data);
        }
    }
}

static void *monitor_thread(void *arg)
{
    (void)arg;
    while (g_mon.running) {
        pthread_mutex_lock(&g_mon.lock);
        uint64_t now = now_ms();
        for (uint32_t i = 0; i < AFROS_MON_MAX_DEVICES; ++i) {
            afros_mon_device_t *d = &g_mon.devices[i];
            if (!d->valid) {
                continue;
            }
            if (now - d->last_seen_ms > AFROS_MON_OFFLINE_MS) {
                dispatch_event_locked(d->device_id, AFROS_DEV_EVENT_OFFLINE);
                d->valid = false;
                if (g_mon.count > 0) {
                    g_mon.count--;
                }
            }
        }
        pthread_mutex_unlock(&g_mon.lock);
        struct timespec ts = { .tv_sec = 0, .tv_nsec = AFROS_MON_TICK_MS * 1000000L };
        nanosleep(&ts, NULL);
    }
    return NULL;
}

/**
 * @brief Initialise the device monitor and spawn the offline-sweep thread.
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t DeviceMonitorInit(void)
{
    if (g_mon.initialised) {
        return AFROS_SUCCESS;
    }
    g_mon.initialised = true;
    g_mon.running     = true;
    if (pthread_create(&g_mon.thread, NULL, monitor_thread, NULL) != 0) {
        g_mon.running = false;
        return AFROS_ERROR;
    }
    return AFROS_SUCCESS;
}

/** Notify the monitor that a device heartbeat has been received. */
int32_t DeviceMonitorNotifyHeartbeat(const char *device_id,
                                     const char *addr,
                                     uint16_t    port)
{
    if (device_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_mon.lock);
    afros_mon_device_t *d = NULL;
    for (uint32_t i = 0; i < AFROS_MON_MAX_DEVICES; ++i) {
        if (g_mon.devices[i].valid &&
            strncmp(g_mon.devices[i].device_id, device_id, 63) == 0) {
            d = &g_mon.devices[i];
            break;
        }
    }
    bool new_device = (d == NULL);
    if (new_device) {
        for (uint32_t i = 0; i < AFROS_MON_MAX_DEVICES; ++i) {
            if (!g_mon.devices[i].valid) {
                d = &g_mon.devices[i];
                memset(d, 0, sizeof(*d));
                d->valid = true;
                strncpy(d->device_id, device_id, 63);
                d->first_seen_ms = now_ms();
                g_mon.count++;
                break;
            }
        }
    }
    if (d == NULL) {
        pthread_mutex_unlock(&g_mon.lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    if (addr != NULL) {
        strncpy(d->addr, addr, 63);
    }
    d->port         = port;
    d->last_seen_ms = now_ms();
    if (new_device) {
        dispatch_event_locked(d->device_id, AFROS_DEV_EVENT_ONLINE);
    }
    pthread_mutex_unlock(&g_mon.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Look up a device by id.
 * @param device_id  Stable device identifier.
 * @param out        Output device record (may be NULL).
 */
int32_t DeviceMonitorGetDevice(const char *device_id, afros_mon_device_t *out)
{
    if (device_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_mon.lock);
    for (uint32_t i = 0; i < AFROS_MON_MAX_DEVICES; ++i) {
        if (g_mon.devices[i].valid &&
            strncmp(g_mon.devices[i].device_id, device_id, 63) == 0) {
            if (out != NULL) {
                *out = g_mon.devices[i];
            }
            pthread_mutex_unlock(&g_mon.lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_mon.lock);
    return AFROS_ERROR;
}
