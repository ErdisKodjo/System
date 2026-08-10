/**
 * @file mdns_discovery.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus mDNS discovery.
 *
 * Publishes the local device as "_afros-softbus._tcp" on the local network
 * and resolves peer instances. The sandbox doesn't ship a real mDNS daemon,
 * so we emulate it: the resolver thread sleeps briefly and returns a static
 * list of two peers.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define AFROS_MDNS_SERVICE     "_afros-softbus._tcp.local."
#define AFROS_MDNS_MAX_PEERS   16
#define AFROS_MDNS_QUERY_MS    500

/** mDNS-resolved peer. */
typedef struct {
    char     instance[32];   /**< "afros-watch" etc. */
    char     addr[64];       /**< IPv4 string. */
    uint16_t port;
    bool     used;
} afros_mdns_peer_t;

typedef void (*afros_mdns_cb_t)(const afros_mdns_peer_t *peer);

static struct {
    bool                advertising;
    bool                resolving;
    pthread_t           thread;
    pthread_mutex_t     lock;
    afros_mdns_peer_t   peers[AFROS_MDNS_MAX_PEERS];
    uint32_t            count;
    afros_mdns_cb_t     cb;
} g_mdns = { .lock = PTHREAD_MUTEX_INITIALIZER };

/**
 * @brief Start advertising the local SoftBus service over mDNS.
 * @param port TCP port to publish (default 6667).
 */
int32_t MdnsDiscoveryStart(uint16_t port)
{
    if (g_mdns.advertising) {
        return AFROS_SUCCESS;
    }
    (void)port;
    g_mdns.advertising = true;
    return AFROS_SUCCESS;
}

/** Background resolver: synthesises two peers after a short delay. */
static void *resolve_worker(void *arg)
{
    (void)arg;
    static const afros_mdns_peer_t kSeeded[] = {
        { "afros-watch",  "10.0.0.12", 6667, true },
        { "afros-tablet", "10.0.0.13", 6667, true },
    };
    for (size_t i = 0; i < sizeof(kSeeded) / sizeof(kSeeded[0]); ++i) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = AFROS_MDNS_QUERY_MS * 1000000L };
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&g_mdns.lock);
        if (g_mdns.count < AFROS_MDNS_MAX_PEERS) {
            g_mdns.peers[g_mdns.count] = kSeeded[i];
            g_mdns.peers[g_mdns.count].used = true;
            if (g_mdns.cb != NULL) {
                g_mdns.cb(&g_mdns.peers[g_mdns.count]);
            }
            g_mdns.count++;
        }
        pthread_mutex_unlock(&g_mdns.lock);
    }
    return NULL;
}

/**
 * @brief Start asynchronous mDNS resolution.
 */
int32_t MdnsDiscoveryResolve(void)
{
    if (g_mdns.resolving) {
        return AFROS_SUCCESS;
    }
    if (pthread_create(&g_mdns.thread, NULL, resolve_worker, NULL) != 0) {
        return AFROS_ERROR;
    }
    g_mdns.resolving = true;
    return AFROS_SUCCESS;
}

/**
 * @brief Stop advertising and resolving; clear the peer cache.
 */
int32_t MdnsDiscoveryStop(void)
{
    if (g_mdns.resolving) {
        g_mdns.resolving = false;
        pthread_join(g_mdns.thread, NULL);
    }
    g_mdns.advertising = false;
    pthread_mutex_lock(&g_mdns.lock);
    memset(g_mdns.peers, 0, sizeof(g_mdns.peers));
    g_mdns.count = 0;
    pthread_mutex_unlock(&g_mdns.lock);
    return AFROS_SUCCESS;
}

/** @brief Register a callback fired whenever a peer is resolved. */
int32_t MdnsDiscoverySetCallback(afros_mdns_cb_t cb)
{
    pthread_mutex_lock(&g_mdns.lock);
    g_mdns.cb = cb;
    pthread_mutex_unlock(&g_mdns.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Snapshot resolved peers into a caller buffer.
 * @param out          Output array (may be NULL to query size).
 * @param count_inout  In: capacity; Out: number written.
 */
int32_t MdnsDiscoveryGetPeers(afros_mdns_peer_t *out, uint32_t *count_inout)
{
    if (count_inout == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_mdns.lock);
    uint32_t cap = *count_inout, n = 0;
    for (uint32_t i = 0; i < AFROS_MDNS_MAX_PEERS && n < cap; ++i) {
        if (g_mdns.peers[i].used) {
            if (out != NULL) {
                out[n] = g_mdns.peers[i];
            }
            n++;
        }
    }
    *count_inout = n;
    pthread_mutex_unlock(&g_mdns.lock);
    return AFROS_SUCCESS;
}

/** @brief Construct the full service name for diagnostics. */
int32_t MdnsDiscoveryGetServiceName(char *out, uint32_t cap)
{
    if (out == NULL || cap == 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    strncpy(out, AFROS_MDNS_SERVICE, cap - 1);
    out[cap - 1] = '\0';
    return AFROS_SUCCESS;
}
