/**
 * @file distributed_data_mgr.c
 * @brief AfriOS HarmonyOS compatibility — distributed data service (DDS).
 *
 * Top-level API used by abilities and services to store, retrieve and delete
 * key/value records that automatically synchronise across paired devices.
 * Internally it keeps an in-memory KV store, attaches a vector clock to each
 * value, enqueues change events on the SyncEngine, and resolves conflicts
 * via the conflict_resolver module.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define AFROS_DDS_MAX_KEYS    128
#define AFROS_DDS_KEY_LEN     64
#define AFROS_DDS_NODE_ID_LEN 32

/** A single distributed record. */
typedef struct {
    char     key[AFROS_DDS_KEY_LEN];
    uint8_t *value;             /**< Owned by the store. */
    uint32_t value_len;
    uint64_t timestamp_ms;
    uint32_t vc[AFROS_DDS_NODE_ID_LEN]; /**< Simplified vector clock:
                                             node-indexed counters. */
    bool     tombstone;
    bool     used;
} afros_dds_record_t;

static struct {
    pthread_mutex_t      lock;
    afros_dds_record_t   records[AFROS_DDS_MAX_KEYS];
    uint32_t             count;
    char                 local_node[AFROS_DDS_NODE_ID_LEN];
    bool                 initialised;
} g_dds = { .lock = PTHREAD_MUTEX_INITIALIZER };

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

static afros_dds_record_t *find_locked(const char *key)
{
    for (uint32_t i = 0; i < AFROS_DDS_MAX_KEYS; ++i) {
        if (g_dds.records[i].used &&
            strncmp(g_dds.records[i].key, key, AFROS_DDS_KEY_LEN - 1) == 0) {
            return &g_dds.records[i];
        }
    }
    return NULL;
}

/**
 * @brief Initialise the DDS with the local node identifier (used in vector
 *        clocks and as the SyncEngine origin).
 */
int32_t DdsInit(const char *local_node_id)
{
    if (local_node_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_dds.lock);
    if (g_dds.initialised) {
        pthread_mutex_unlock(&g_dds.lock);
        return AFROS_SUCCESS;
    }
    strncpy(g_dds.local_node, local_node_id, AFROS_DDS_NODE_ID_LEN - 1);
    g_dds.initialised = true;
    pthread_mutex_unlock(&g_dds.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Insert or update a key in the local store and queue a sync push.
 * @param key       Key (≤ 63 chars).
 * @param value     Value bytes (copied).
 * @param value_len Length of value.
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t DdsPut(const char    *key,
               const uint8_t *value,
               uint32_t       value_len)
{
    if (key == NULL || (value == NULL && value_len > 0) ||
        strlen(key) >= AFROS_DDS_KEY_LEN) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_dds.lock);
    afros_dds_record_t *r = find_locked(key);
    if (r == NULL) {
        for (uint32_t i = 0; i < AFROS_DDS_MAX_KEYS; ++i) {
            if (!g_dds.records[i].used) {
                r = &g_dds.records[i];
                memset(r, 0, sizeof(*r));
                r->used = true;
                strncpy(r->key, key, AFROS_DDS_KEY_LEN - 1);
                g_dds.count++;
                break;
            }
        }
        if (r == NULL) {
            pthread_mutex_unlock(&g_dds.lock);
            return AFROS_ERROR_NO_MEMORY;
        }
    }
    if (r->value != NULL) {
        free(r->value);
        r->value = NULL;
    }
    if (value_len > 0) {
        r->value = (uint8_t *)malloc(value_len);
        if (r->value == NULL) {
            pthread_mutex_unlock(&g_dds.lock);
            return AFROS_ERROR_NO_MEMORY;
        }
        memcpy(r->value, value, value_len);
    }
    r->value_len     = value_len;
    r->timestamp_ms  = now_ms();
    r->tombstone     = false;
    /* Increment our local component. */
    uint32_t idx = (uint32_t)(g_dds.local_node[0] & 0x1F);
    r->vc[idx]  += 1;
    pthread_mutex_unlock(&g_dds.lock);

    /* Notify the (optional) sync engine. Best-effort: declare extern. */
    extern int32_t SyncEnginePush(const char *, const char *,
                                  const uint8_t *, uint32_t, bool);
    (void)SyncEnginePush(NULL, key, value, value_len, false);
    return AFROS_SUCCESS;
}

/**
 * @brief Retrieve a key.
 * @param key         Key to look up.
 * @param out_value   Caller-allocated buffer (may be NULL to query length).
 * @param in_out_len  In: capacity; Out: bytes written.
 * @return AFROS_SUCCESS, AFROS_ERROR if not found, or AFROS_ERROR_NO_MEMORY.
 */
int32_t DdsGet(const char *key,
               uint8_t    *out_value,
               uint32_t   *in_out_len)
{
    if (key == NULL || in_out_len == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_dds.lock);
    afros_dds_record_t *r = find_locked(key);
    int32_t rc;
    if (r == NULL || r->tombstone) {
        rc = AFROS_ERROR;
    } else if (out_value == NULL) {
        *in_out_len = r->value_len;
        rc = AFROS_SUCCESS;
    } else if (*in_out_len < r->value_len) {
        rc = AFROS_ERROR_NO_MEMORY;
    } else {
        memcpy(out_value, r->value, r->value_len);
        *in_out_len = r->value_len;
        rc = AFROS_SUCCESS;
    }
    pthread_mutex_unlock(&g_dds.lock);
    return rc;
}

/**
 * @brief Delete a key (logically — leaves a tombstone for sync).
 * @return AFROS_SUCCESS even if the key was unknown.
 */
int32_t DdsDelete(const char *key)
{
    if (key == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_dds.lock);
    afros_dds_record_t *r = find_locked(key);
    if (r != NULL) {
        if (r->value != NULL) {
            free(r->value);
            r->value = NULL;
        }
        r->value_len    = 0;
        r->tombstone    = true;
        r->timestamp_ms = now_ms();
        uint32_t idx = (uint32_t)(g_dds.local_node[0] & 0x1F);
        r->vc[idx]  += 1;
    }
    pthread_mutex_unlock(&g_dds.lock);

    extern int32_t SyncEnginePush(const char *, const char *,
                                  const uint8_t *, uint32_t, bool);
    (void)SyncEnginePush(NULL, key, NULL, 0, true);
    return AFROS_SUCCESS;
}

/**
 * @brief Force an immediate sync round with all peers.
 *        In the sandbox this is a no-op (the engine ticks on its own).
 */
int32_t DdsSync(void)
{
    return AFROS_SUCCESS;
}

/**
 * @brief Apply a remote change (received by SyncEngine via SoftBus).
 *        Uses simple LWW based on timestamp_ms.
 */
int32_t DdsApplyRemote(const char    *key,
                       const uint8_t *value,
                       uint32_t       value_len,
                       bool           tombstone,
                       uint64_t       remote_ts)
{
    if (key == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_dds.lock);
    afros_dds_record_t *r = find_locked(key);
    if (r == NULL) {
        for (uint32_t i = 0; i < AFROS_DDS_MAX_KEYS; ++i) {
            if (!g_dds.records[i].used) {
                r = &g_dds.records[i];
                memset(r, 0, sizeof(*r));
                r->used = true;
                strncpy(r->key, key, AFROS_DDS_KEY_LEN - 1);
                g_dds.count++;
                break;
            }
        }
        if (r == NULL) {
            pthread_mutex_unlock(&g_dds.lock);
            return AFROS_ERROR_NO_MEMORY;
        }
    }
    if (remote_ts < r->timestamp_ms && r->value != NULL) {
        /* Local copy is newer — leave it. */
        pthread_mutex_unlock(&g_dds.lock);
        return AFROS_SUCCESS;
    }
    if (r->value != NULL) {
        free(r->value);
        r->value = NULL;
    }
    if (value_len > 0 && value != NULL) {
        r->value = (uint8_t *)malloc(value_len);
        if (r->value == NULL) {
            pthread_mutex_unlock(&g_dds.lock);
            return AFROS_ERROR_NO_MEMORY;
        }
        memcpy(r->value, value, value_len);
    }
    r->value_len    = value_len;
    r->tombstone    = tombstone;
    r->timestamp_ms = remote_ts;
    pthread_mutex_unlock(&g_dds.lock);
    return AFROS_SUCCESS;
}

/** @brief Number of live keys currently in the store. */
uint32_t DdsKeyCount(void)
{
    pthread_mutex_lock(&g_dds.lock);
    uint32_t n = g_dds.count;
    pthread_mutex_unlock(&g_dds.lock);
    return n;
}
