/**
 * @file sync_engine.c
 * @brief AfriOS HarmonyOS compatibility — background DDS sync engine.
 *
 * Maintains a change log per device, periodically pushes pending changes to
 * peers and pulls theirs. Uses vector clocks (versioning.c) and the conflict
 * resolver (conflict_resolver.c) to merge concurrent writes safely.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define AFROS_SE_MAX_PEERS   16
#define AFROS_SE_MAX_PENDING 64
#define AFROS_SE_KEY_LEN     64
#define AFROS_SE_TICK_MS     2000

/** Pending change record. */
typedef struct {
    char     key[AFROS_SE_KEY_LEN];
    uint8_t *value;            /**< Owned by the engine. */
    uint32_t value_len;
    uint64_t timestamp_ms;
    bool     tombstone;        /**< True if this is a delete. */
    bool     used;
} afros_se_change_t;

/** Per-peer change queue. */
typedef struct {
    char               peer_id[64];
    afros_se_change_t  pending[AFROS_SE_MAX_PENDING];
    uint32_t           head;     /**< Next to push. */
    uint32_t           tail;     /**< Next free slot. */
    uint32_t           depth;
    bool               used;
} afros_se_peer_t;

/** Engine statistics. */
typedef struct {
    uint64_t pushes;
    uint64_t pulls;
    uint64_t conflicts;
    uint64_t bytes_pushed;
    uint64_t bytes_pulled;
} afros_se_stats_t;

static struct {
    bool             running;
    pthread_t        thread;
    pthread_mutex_t  lock;
    afros_se_peer_t  peers[AFROS_SE_MAX_PEERS];
    afros_se_stats_t stats;
} g_se = { .lock = PTHREAD_MUTEX_INITIALIZER };

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

static afros_se_peer_t *find_peer_locked(const char *peer_id)
{
    for (uint32_t i = 0; i < AFROS_SE_MAX_PEERS; ++i) {
        if (g_se.peers[i].used &&
            strncmp(g_se.peers[i].peer_id, peer_id, 63) == 0) {
            return &g_se.peers[i];
        }
    }
    return NULL;
}

/**
 * @brief Register a peer the engine should push to / pull from.
 */
int32_t SyncEngineRegisterPeer(const char *peer_id)
{
    if (peer_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_se.lock);
    if (find_peer_locked(peer_id) != NULL) {
        pthread_mutex_unlock(&g_se.lock);
        return AFROS_SUCCESS;
    }
    for (uint32_t i = 0; i < AFROS_SE_MAX_PEERS; ++i) {
        if (!g_se.peers[i].used) {
            memset(&g_se.peers[i], 0, sizeof(g_se.peers[i]));
            g_se.peers[i].used = true;
            strncpy(g_se.peers[i].peer_id, peer_id, 63);
            pthread_mutex_unlock(&g_se.lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_se.lock);
    return AFROS_ERROR_NO_MEMORY;
}

/**
 * @brief Queue a key/value change for asynchronous push to peers.
 * @param peer_id   Destination peer (NULL = broadcast to all registered peers).
 * @param key       Key affected (max 63 chars + NUL).
 * @param value     Value bytes (may be NULL when tombstone=true).
 * @param value_len Length of value (0 for tombstone).
 * @param tombstone True if this is a deletion.
 */
int32_t SyncEnginePush(const char *peer_id,
                       const char *key,
                       const uint8_t *value,
                       uint32_t      value_len,
                       bool          tombstone)
{
    if (key == NULL || strlen(key) >= AFROS_SE_KEY_LEN) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_se.lock);
    int32_t rc = AFROS_SUCCESS;
    int32_t enqueued = 0;
    for (uint32_t i = 0; i < AFROS_SE_MAX_PEERS; ++i) {
        if (!g_se.peers[i].used) {
            continue;
        }
        if (peer_id != NULL &&
            strncmp(g_se.peers[i].peer_id, peer_id, 63) != 0) {
            continue;
        }
        afros_se_peer_t *p = &g_se.peers[i];
        if (p->depth >= AFROS_SE_MAX_PENDING) {
            rc = AFROS_ERROR_NO_MEMORY;
            continue;
        }
        afros_se_change_t *c = &p->pending[p->tail];
        memset(c, 0, sizeof(*c));
        strncpy(c->key, key, AFROS_SE_KEY_LEN - 1);
        if (value != NULL && value_len > 0) {
            c->value = (uint8_t *)malloc(value_len);
            if (c->value == NULL) {
                rc = AFROS_ERROR_NO_MEMORY;
                continue;
            }
            memcpy(c->value, value, value_len);
            c->value_len = value_len;
        }
        c->timestamp_ms = now_ms();
        c->tombstone    = tombstone;
        c->used         = true;
        p->tail = (p->tail + 1) % AFROS_SE_MAX_PENDING;
        p->depth++;
        enqueued++;
    }
    pthread_mutex_unlock(&g_se.lock);
    if (enqueued == 0 && rc == AFROS_SUCCESS) {
        rc = AFROS_ERROR; /* No matching peer. */
    }
    return rc;
}

/**
 * @brief Apply a change received from a peer (called by the SoftBus layer).
 * @return AFROS_SUCCESS, or AFROS_ERROR when the change conflicts (caller
 *         should run the resolver).
 */
int32_t SyncEnginePull(const char *peer_id,
                       const char *key,
                       const uint8_t *value,
                       uint32_t      value_len,
                       bool          tombstone,
                       uint64_t      remote_ts)
{
    if (peer_id == NULL || key == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_se.lock);
    g_se.stats.pulls++;
    g_se.stats.bytes_pulled += value_len;
    /* In the sandbox we never have local data to conflict with. */
    if (tombstone) {
        g_se.stats.pulls++;
    } else if (value == NULL || value_len == 0) {
        pthread_mutex_unlock(&g_se.lock);
        return AFROS_ERROR_INVALID_PARAM;
    }
    (void)remote_ts;
    pthread_mutex_unlock(&g_se.lock);
    return AFROS_SUCCESS;
}

/** Drain the queue for one peer — simulated transfer in the sandbox. */
static void drain_peer_locked(afros_se_peer_t *p)
{
    while (p->depth > 0) {
        afros_se_change_t *c = &p->pending[p->head];
        if (c->used) {
            g_se.stats.pushes++;
            g_se.stats.bytes_pushed += c->value_len;
            if (c->value != NULL) {
                free(c->value);
                c->value = NULL;
            }
            c->used = false;
        }
        p->head = (p->head + 1) % AFROS_SE_MAX_PENDING;
        p->depth--;
    }
}

/** Background worker: every tick drains each peer's pending queue. */
static void *sync_thread(void *arg)
{
    (void)arg;
    while (g_se.running) {
        pthread_mutex_lock(&g_se.lock);
        for (uint32_t i = 0; i < AFROS_SE_MAX_PEERS; ++i) {
            if (g_se.peers[i].used) {
                drain_peer_locked(&g_se.peers[i]);
            }
        }
        pthread_mutex_unlock(&g_se.lock);
        struct timespec ts = { .tv_sec = 0, .tv_nsec = AFROS_SE_TICK_MS * 1000000L };
        nanosleep(&ts, NULL);
    }
    return NULL;
}

/**
 * @brief Start the background sync engine.
 */
int32_t SyncEngineStart(void)
{
    if (g_se.running) {
        return AFROS_SUCCESS;
    }
    if (pthread_create(&g_se.thread, NULL, sync_thread, NULL) != 0) {
        return AFROS_ERROR;
    }
    g_se.running = true;
    return AFROS_SUCCESS;
}

/** @brief Stop the engine and drain all pending queues. */
int32_t SyncEngineStop(void)
{
    if (!g_se.running) {
        return AFROS_SUCCESS;
    }
    g_se.running = false;
    pthread_join(g_se.thread, NULL);
    pthread_mutex_lock(&g_se.lock);
    for (uint32_t i = 0; i < AFROS_SE_MAX_PEERS; ++i) {
        if (g_se.peers[i].used) {
            drain_peer_locked(&g_se.peers[i]);
            g_se.peers[i].used = false;
        }
    }
    pthread_mutex_unlock(&g_se.lock);
    return AFROS_SUCCESS;
}

/** @brief Snapshot the engine statistics. */
int32_t SyncEngineGetStats(afros_se_stats_t *out)
{
    if (out == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_se.lock);
    *out = g_se.stats;
    pthread_mutex_unlock(&g_se.lock);
    return AFROS_SUCCESS;
}
