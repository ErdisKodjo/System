/**
 * @file conflict_resolver.c
 * @brief AfriOS HarmonyOS compatibility — DDS conflict resolution.
 *
 * When SyncEngine detects concurrent versions of the same key it consults
 * this module. Default policies: Last-Writer-Wins (using wall-clock
 * timestamps) or merge (concatenate byte values). Custom handlers can be
 * registered per-key-prefix.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define AFROS_CR_MAX_HANDLERS 16
#define AFROS_CR_PREFIX_LEN   64

/** Conflict-resolution policies. */
typedef enum {
    AFROS_CR_LWW     = 0,  /**< Last-writer-wins by timestamp. */
    AFROS_CR_MERGE   = 1,  /**< Concatenate remote onto local. */
    AFROS_CR_CUSTOM  = 2,  /**< Invoke registered handler. */
} afros_cr_policy_t;

/** Candidate value offered to the resolver. */
typedef struct {
    const uint8_t *data;
    uint32_t       len;
    uint64_t       timestamp_ms;  /**< Local write time at origin. */
    char           origin_node[32];
} afros_cr_value_t;

/** Result the resolver produces. */
typedef struct {
    uint8_t *data;        /**< Caller must free(). */
    uint32_t len;
} afros_cr_result_t;

/** Custom handler signature. */
typedef int32_t (*afros_cr_handler_t)(const char *key,
                                      const afros_cr_value_t *local,
                                      const afros_cr_value_t *remote,
                                      afros_cr_result_t       *out);

typedef struct {
    char                prefix[AFROS_CR_PREFIX_LEN];
    afros_cr_handler_t  handler;
    bool                used;
} afros_cr_handler_slot_t;

static struct {
    pthread_mutex_t            lock;
    afros_cr_handler_slot_t    handlers[AFROS_CR_MAX_HANDLERS];
    afros_cr_policy_t          default_policy;
} g_cr = {
    .lock           = PTHREAD_MUTEX_INITIALIZER,
    .default_policy = AFROS_CR_LWW,
};

static afros_cr_handler_t find_handler_locked(const char *key)
{
    uint32_t best_len = 0;
    afros_cr_handler_t best = NULL;
    for (uint32_t i = 0; i < AFROS_CR_MAX_HANDLERS; ++i) {
        if (!g_cr.handlers[i].used) {
            continue;
        }
        uint32_t plen = (uint32_t)strlen(g_cr.handlers[i].prefix);
        if (plen > best_len && strncmp(key, g_cr.handlers[i].prefix, plen) == 0) {
            best     = g_cr.handlers[i].handler;
            best_len = plen;
        }
    }
    return best;
}

/** Allocate a copy of the value into the result. */
static int32_t copy_to_result(const afros_cr_value_t *v, afros_cr_result_t *out)
{
    if (v == NULL || v->data == NULL || v->len == 0) {
        out->data = NULL;
        out->len  = 0;
        return AFROS_SUCCESS;
    }
    out->data = (uint8_t *)malloc(v->len);
    if (out->data == NULL) {
        return AFROS_ERROR_NO_MEMORY;
    }
    memcpy(out->data, v->data, v->len);
    out->len = v->len;
    return AFROS_SUCCESS;
}

/**
 * @brief Resolve a conflict between two concurrent versions of @p key.
 *
 * Dispatch policy:
 *   - LWW: pick the value with the larger timestamp_ms (ties → local).
 *   - MERGE: local followed by remote.
 *   - CUSTOM: invoke the longest-prefix-matching handler.
 *
 * @param key     Affected key.
 * @param local   Locally-held value.
 * @param remote  Incoming value from a peer.
 * @param out     Receives the resolved value (caller frees out->data).
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t ConflictResolve(const char               *key,
                        const afros_cr_value_t   *local,
                        const afros_cr_value_t   *remote,
                        afros_cr_result_t        *out)
{
    if (key == NULL || out == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    out->data = NULL;
    out->len  = 0;

    pthread_mutex_lock(&g_cr.lock);
    afros_cr_handler_t handler = find_handler_locked(key);
    afros_cr_policy_t  policy  = (handler != NULL) ? AFROS_CR_CUSTOM
                                                   : g_cr.default_policy;
    pthread_mutex_unlock(&g_cr.lock);

    switch (policy) {
    case AFROS_CR_CUSTOM:
        return handler(key, local, remote, out);

    case AFROS_CR_MERGE: {
        uint32_t total = (local ? local->len : 0) + (remote ? remote->len : 0);
        if (total == 0) {
            return AFROS_SUCCESS;
        }
        out->data = (uint8_t *)malloc(total);
        if (out->data == NULL) {
            return AFROS_ERROR_NO_MEMORY;
        }
        uint32_t off = 0;
        if (local != NULL && local->len > 0) {
            memcpy(out->data + off, local->data, local->len);
            off += local->len;
        }
        if (remote != NULL && remote->len > 0) {
            memcpy(out->data + off, remote->data, remote->len);
        }
        out->len = total;
        return AFROS_SUCCESS;
    }

    case AFROS_CR_LWW:
    default: {
        uint64_t lt = local  ? local->timestamp_ms  : 0;
        uint64_t rt = remote ? remote->timestamp_ms : 0;
        const afros_cr_value_t *winner = (rt > lt) ? remote : local;
        return copy_to_result(winner, out);
    }
    }
}

/**
 * @brief Register a custom conflict handler for keys with the given prefix.
 * @param prefix  Key prefix this handler applies to (longest-prefix wins).
 * @param handler Handler function.
 * @return AFROS_SUCCESS or AFROS_ERROR_NO_MEMORY when the table is full.
 */
int32_t ConflictRegisterHandler(const char         *prefix,
                                afros_cr_handler_t  handler)
{
    if (prefix == NULL || handler == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_cr.lock);
    /* Reuse an existing slot if the prefix is already registered. */
    for (uint32_t i = 0; i < AFROS_CR_MAX_HANDLERS; ++i) {
        if (g_cr.handlers[i].used &&
            strncmp(g_cr.handlers[i].prefix, prefix,
                    AFROS_CR_PREFIX_LEN) == 0) {
            g_cr.handlers[i].handler = handler;
            pthread_mutex_unlock(&g_cr.lock);
            return AFROS_SUCCESS;
        }
    }
    for (uint32_t i = 0; i < AFROS_CR_MAX_HANDLERS; ++i) {
        if (!g_cr.handlers[i].used) {
            g_cr.handlers[i].used = true;
            strncpy(g_cr.handlers[i].prefix, prefix, AFROS_CR_PREFIX_LEN - 1);
            g_cr.handlers[i].handler = handler;
            pthread_mutex_unlock(&g_cr.lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_cr.lock);
    return AFROS_ERROR_NO_MEMORY;
}

/**
 * @brief Set the default resolution policy (used when no handler matches).
 */
int32_t ConflictSetDefaultPolicy(afros_cr_policy_t policy)
{
    if (policy > AFROS_CR_CUSTOM) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_cr.lock);
    g_cr.default_policy = policy;
    pthread_mutex_unlock(&g_cr.lock);
    return AFROS_SUCCESS;
}

/** @brief Free a value previously produced by ConflictResolve. */
void ConflictFreeResult(afros_cr_result_t *out)
{
    if (out != NULL && out->data != NULL) {
        free(out->data);
        out->data = NULL;
        out->len  = 0;
    }
}
