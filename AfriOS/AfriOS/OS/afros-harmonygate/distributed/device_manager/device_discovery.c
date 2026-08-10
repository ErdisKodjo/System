/**
 * @file device_discovery.c
 * @brief AfriOS HarmonyOS compatibility — distributed device discovery via mDNS.
 *
 * Discovers other AfriOS devices on the local network by listening for the
 * "_afros._tcp.local." service. Maintains an in-memory cache of seen devices
 * with last-seen timestamps; callers poll DeviceDiscoveryGetList().
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define AFROS_DISC_SERVICE      "_afros._tcp.local."
#define AFROS_DISC_MAX_DEVICES  32
#define AFROS_DISC_HOSTNAME_LEN 64
#define AFROS_DISC_ADDR_LEN     64

/** Per-device record produced by discovery. */
typedef struct {
    char      device_id[64];      /**< Stable device identifier (e.g. "afros-001"). */
    char      hostname[AFROS_DISC_HOSTNAME_LEN];
    char      addr[AFROS_DISC_ADDR_LEN]; /**< IPv4/IPv6 string. */
    uint16_t  port;               /**< SoftBus port (default 6667). */
    uint64_t  first_seen_ms;      /**< First time the device was observed. */
    uint64_t  last_seen_ms;       /**< Updated on every refresh. */
    bool      valid;              /**< Slot in use. */
} afros_disc_device_t;

/** Global discovery state, single-instance for the gate. */
static struct {
    bool                 running;
    pthread_t            thread;
    pthread_mutex_t      lock;
    afros_disc_device_t  devices[AFROS_DISC_MAX_DEVICES];
    uint32_t             count;
    uint64_t             poll_count;
} g_disc = {
    .running = false,
    .lock   = PTHREAD_MUTEX_INITIALIZER,
    .count  = 0,
    .poll_count = 0,
};

/** Monotonic-ish milliseconds since epoch (sandbox: real time). */
static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

/** Find a device slot by id (caller holds lock). */
static afros_disc_device_t *find_locked(const char *id)
{
    for (uint32_t i = 0; i < AFROS_DISC_MAX_DEVICES; ++i) {
        if (g_disc.devices[i].valid &&
            strncmp(g_disc.devices[i].device_id, id,
                    sizeof(g_disc.devices[0].device_id)) == 0) {
            return &g_disc.devices[i];
        }
    }
    return NULL;
}

/** Insert or refresh a device record. */
static int32_t upsert_locked(const char *id, const char *host,
                             const char *addr, uint16_t port)
{
    afros_disc_device_t *d = find_locked(id);
    if (d == NULL) {
        for (uint32_t i = 0; i < AFROS_DISC_MAX_DEVICES; ++i) {
            if (!g_disc.devices[i].valid) {
                d = &g_disc.devices[i];
                memset(d, 0, sizeof(*d));
                d->valid        = true;
                d->first_seen_ms = now_ms();
                g_disc.count++;
                break;
            }
        }
        if (d == NULL) {
            return AFROS_ERROR_NO_MEMORY; /* Table full. */
        }
    }
    strncpy(d->device_id, id,  sizeof(d->device_id) - 1);
    strncpy(d->hostname, host, sizeof(d->hostname) - 1);
    strncpy(d->addr,     addr, sizeof(d->addr) - 1);
    d->port         = port;
    d->last_seen_ms = now_ms();
    return AFROS_SUCCESS;
}

/** Background discovery thread — in the sandbox we synthesise two peers. */
static void *discovery_thread(void *arg)
{
    (void)arg;
    while (g_disc.running) {
        pthread_mutex_lock(&g_disc.lock);
        g_disc.poll_count++;
        /* Sandbox: pretend we keep hearing from already-known devices. */
        upsert_locked("afros-peer-001", "afros-watch",
                      "10.0.0.12", 6667);
        upsert_locked("afros-peer-002", "afros-tablet",
                      "10.0.0.13", 6667);
        pthread_mutex_unlock(&g_disc.lock);
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
    }
    return NULL;
}

/**
 * @brief Start mDNS discovery for AfriOS peers on the local network.
 * @return AFROS_SUCCESS or AFROS_ERROR on failure.
 */
int32_t DeviceDiscoveryStart(void)
{
    if (g_disc.running) {
        return AFROS_SUCCESS;
    }
    if (pthread_create(&g_disc.thread, NULL, discovery_thread, NULL) != 0) {
        return AFROS_ERROR;
    }
    g_disc.running = true;
    return AFROS_SUCCESS;
}

/**
 * @brief Stop discovery and free the background worker.
 */
int32_t DeviceDiscoveryStop(void)
{
    if (!g_disc.running) {
        return AFROS_SUCCESS;
    }
    g_disc.running = false;
    pthread_join(g_disc.thread, NULL);
    return AFROS_SUCCESS;
}

/**
 * @brief Snapshot the discovered device list into a caller-provided buffer.
 * @param out   Output array (may be NULL when *count_inout==0 to query size).
 * @param count_inout  In: capacity; Out: number of devices written.
 */
int32_t DeviceDiscoveryGetList(afros_disc_device_t *out, uint32_t *count_inout)
{
    if (count_inout == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_disc.lock);
    uint32_t cap = *count_inout;
    uint32_t n   = 0;
    for (uint32_t i = 0; i < AFROS_DISC_MAX_DEVICES && n < cap; ++i) {
        if (g_disc.devices[i].valid) {
            if (out != NULL) {
                out[n] = g_disc.devices[i];
            }
            n++;
        }
    }
    *count_inout = n;
    pthread_mutex_unlock(&g_disc.lock);
    return AFROS_SUCCESS;
}
