/**
 * @file ble_discovery.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus BLE discovery.
 *
 * Advertises the local device over BLE and scans for peer advertisements.
 * In the sandbox we don't have a real BLE controller, so we synthesise
 * a fixed 128-bit service UUID and a 6-byte device id payload, and
 * pretend to see two peers after a short scan window.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define AFROS_BLE_SCAN_MS    3000
#define AFROS_BLE_MAX_PEERS  16
#define AFROS_BLE_DEV_ID_LEN 7   /* 6 bytes + NUL. */

/** Discovered BLE peer. */
typedef struct {
    char     device_id[AFROS_BLE_DEV_ID_LEN];
    int8_t   rssi;
    bool     used;
} afros_ble_peer_t;

/** Callback invoked when a new peer is seen. */
typedef void (*afros_ble_peer_cb_t)(const char *device_id, int8_t rssi);

static struct {
    bool              advertising;
    bool              scanning;
    pthread_t         scan_thread;
    afros_ble_peer_t  peers[AFROS_BLE_MAX_PEERS];
    uint32_t          peer_count;
    afros_ble_peer_cb_t cb;
    pthread_mutex_t   lock;
} g_ble = { .lock = PTHREAD_MUTEX_INITIALIZER };

/** Synthesised advertisement payload: 6 ASCII bytes of the local id. */
static int32_t build_adv_payload(uint8_t *out, uint32_t cap)
{
    static const char local_id[6] = { 'A','F','R','0','0','1' };
    if (cap < sizeof(local_id)) {
        return AFROS_ERROR_NO_MEMORY;
    }
    memcpy(out, local_id, sizeof(local_id));
    return (int32_t)sizeof(local_id);
}

/**
 * @brief Start BLE advertising so peers can discover us.
 * @return AFROS_SUCCESS or AFROS_ERROR on failure.
 */
int32_t BleDiscoveryStart(void)
{
    if (g_ble.advertising) {
        return AFROS_SUCCESS;
    }
    uint8_t buf[16];
    int32_t n = build_adv_payload(buf, sizeof(buf));
    if (n < 0) {
        return AFROS_ERROR_NO_MEMORY;
    }
    g_ble.advertising = true;
    return AFROS_SUCCESS;
}

/** Background scan: pretend to discover two peers after a short delay. */
static void *scan_worker(void *arg)
{
    (void)arg;
    static const char *kPeers[] = { "AFR002", "AFR003" };
    for (size_t i = 0; i < sizeof(kPeers) / sizeof(kPeers[0]); ++i) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000000L };
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&g_ble.lock);
        if (g_ble.peer_count < AFROS_BLE_MAX_PEERS) {
            afros_ble_peer_t *p = &g_ble.peers[g_ble.peer_count++];
            memset(p, 0, sizeof(*p));
            memcpy(p->device_id, kPeers[i], 6);
            p->device_id[6] = '\0';
            p->rssi = -50 - (int8_t)(i * 10);
            p->used = true;
            if (g_ble.cb != NULL) {
                g_ble.cb(p->device_id, p->rssi);
            }
        }
        pthread_mutex_unlock(&g_ble.lock);
    }
    return NULL;
}

/**
 * @brief Start a BLE scan. Returns immediately; results are delivered via
 *        the callback registered with BleDiscoverySetCallback.
 */
int32_t BleDiscoveryScanStart(void)
{
    if (g_ble.scanning) {
        return AFROS_SUCCESS;
    }
    if (pthread_create(&g_ble.scan_thread, NULL, scan_worker, NULL) != 0) {
        return AFROS_ERROR;
    }
    g_ble.scanning = true;
    return AFROS_SUCCESS;
}

/**
 * @brief Stop both advertising and scanning and free resources.
 */
int32_t BleDiscoveryStop(void)
{
    if (g_ble.scanning) {
        g_ble.scanning = false;
        pthread_join(g_ble.scan_thread, NULL);
    }
    g_ble.advertising = false;
    pthread_mutex_lock(&g_ble.lock);
    memset(g_ble.peers, 0, sizeof(g_ble.peers));
    g_ble.peer_count = 0;
    pthread_mutex_unlock(&g_ble.lock);
    return AFROS_SUCCESS;
}

/** @brief Register the peer-discovered callback. */
int32_t BleDiscoverySetCallback(afros_ble_peer_cb_t cb)
{
    pthread_mutex_lock(&g_ble.lock);
    g_ble.cb = cb;
    pthread_mutex_unlock(&g_ble.lock);
    return AFROS_SUCCESS;
}

/** @brief Snapshot the list of discovered peers. */
int32_t BleDiscoveryGetPeers(afros_ble_peer_t *out, uint32_t *count_inout)
{
    if (count_inout == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_ble.lock);
    uint32_t cap = *count_inout, n = 0;
    for (uint32_t i = 0; i < AFROS_BLE_MAX_PEERS && n < cap; ++i) {
        if (g_ble.peers[i].used) {
            if (out != NULL) {
                out[n] = g_ble.peers[i];
            }
            n++;
        }
    }
    *count_inout = n;
    pthread_mutex_unlock(&g_ble.lock);
    return AFROS_SUCCESS;
}
