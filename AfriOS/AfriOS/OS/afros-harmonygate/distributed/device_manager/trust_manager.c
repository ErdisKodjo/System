/**
 * @file trust_manager.c
 * @brief AfriOS HarmonyOS compatibility — inter-device trust relationships.
 *
 * Implements PIN-based pairing: the initiating device displays a 6-digit PIN,
 * the user enters it on the peer; if both sides agree on the PIN a trust
 * record is created and persisted. Trusted devices can exchange session
 * keys without re-prompting.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define AFROS_TRUST_MAX_DEVICES 32
#define AFROS_TRUST_PIN_LEN     8      /**< 6 digits + NUL + slack. */
#define AFROS_TRUST_KEY_LEN     32     /**< Long-term shared secret length. */

/** State of a pairing attempt. */
typedef enum {
    AFROS_PAIR_IDLE = 0,
    AFROS_PAIR_PENDING,
    AFROS_PAIR_CONFIRMED,
    AFROS_PAIR_REJECTED,
} afros_pair_state_t;

typedef struct {
    char     device_id[64];
    char     friendly_name[64];
    char     pin[AFROS_TRUST_PIN_LEN];
    uint8_t  ltk[AFROS_TRUST_KEY_LEN]; /**< Long-term key. */
    uint64_t created_ms;
    uint64_t last_used_ms;
    afros_pair_state_t state;
    bool     valid;
} afros_trust_record_t;

typedef struct {
    char     peer_id[64];
    char     pin[AFROS_TRUST_PIN_LEN];
    uint64_t expires_ms;
    bool     active;
} afros_pair_attempt_t;

static struct {
    pthread_mutex_t       lock;
    afros_trust_record_t  records[AFROS_TRUST_MAX_DEVICES];
    uint32_t              count;
    afros_pair_attempt_t  pending;
} g_trust = { .lock = PTHREAD_MUTEX_INITIALIZER };

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

/** Generate a fresh 6-digit PIN string. */
static void generate_pin(char *out, size_t len)
{
    uint32_t v = (uint32_t)rand() % 1000000u;
    snprintf(out, len, "%06u", v);
}

/** Generate a deterministic long-term key from PIN + device ids (sandbox). */
static void derive_ltk(const char *pin, const char *peer, uint8_t *out)
{
    /* FNV-1a-ish expansion to 32 bytes. */
    uint32_t h = 2166136261u;
    for (const char *p = pin; *p; ++p) {
        h ^= (uint8_t)*p; h *= 16777619u;
    }
    for (const char *p = peer; *p; ++p) {
        h ^= (uint8_t)*p; h *= 16777619u;
    }
    for (uint32_t i = 0; i < AFROS_TRUST_KEY_LEN; ++i) {
        h ^= h >> 13; h *= 0x85ebca6bu;
        out[i] = (uint8_t)(h >> 24);
    }
}

static afros_trust_record_t *find_locked(const char *device_id)
{
    for (uint32_t i = 0; i < AFROS_TRUST_MAX_DEVICES; ++i) {
        if (g_trust.records[i].valid &&
            strncmp(g_trust.records[i].device_id, device_id, 63) == 0) {
            return &g_trust.records[i];
        }
    }
    return NULL;
}

/**
 * @brief Initiate pairing with a peer device.
 * @param peer_id    Peer device identifier.
 * @param friendly   Optional friendly name.
 * @param out_pin    Receives the generated PIN (6 digits + NUL).
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t TrustRequestPair(const char *peer_id,
                         const char *friendly,
                         char       *out_pin)
{
    if (peer_id == NULL || out_pin == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_trust.lock);
    generate_pin(g_trust.pending.pin, AFROS_TRUST_PIN_LEN);
    strncpy(g_trust.pending.peer_id, peer_id, 63);
    g_trust.pending.expires_ms = now_ms() + 60000; /* 60 s window. */
    g_trust.pending.active     = true;
    strncpy(out_pin, g_trust.pending.pin, AFROS_TRUST_PIN_LEN);

    /* Pre-create the trust record in PENDING state. */
    afros_trust_record_t *r = find_locked(peer_id);
    if (r == NULL) {
        for (uint32_t i = 0; i < AFROS_TRUST_MAX_DEVICES; ++i) {
            if (!g_trust.records[i].valid) {
                r = &g_trust.records[i];
                memset(r, 0, sizeof(*r));
                r->valid = true;
                strncpy(r->device_id, peer_id, 63);
                g_trust.count++;
                break;
            }
        }
    }
    if (r != NULL) {
        r->state = AFROS_PAIR_PENDING;
        r->created_ms = now_ms();
        if (friendly != NULL) {
            strncpy(r->friendly_name, friendly, 63);
        }
    }
    pthread_mutex_unlock(&g_trust.lock);
    return (r != NULL) ? AFROS_SUCCESS : AFROS_ERROR_NO_MEMORY;
}

/**
 * @brief Confirm a pairing using the PIN the user entered on the peer.
 * @param peer_id  Peer device identifier.
 * @param pin      PIN as entered on the peer.
 * @return AFROS_SUCCESS if paired, AFROS_ERROR otherwise.
 */
int32_t TrustConfirmPair(const char *peer_id, const char *pin)
{
    if (peer_id == NULL || pin == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_trust.lock);
    int32_t rc;
    if (!g_trust.pending.active ||
        strncmp(g_trust.pending.peer_id, peer_id, 63) != 0 ||
        strncmp(g_trust.pending.pin, pin, AFROS_TRUST_PIN_LEN) != 0 ||
        now_ms() > g_trust.pending.expires_ms) {
        rc = AFROS_ERROR;
    } else {
        afros_trust_record_t *r = find_locked(peer_id);
        if (r != NULL) {
            r->state = AFROS_PAIR_CONFIRMED;
            r->last_used_ms = now_ms();
            derive_ltk(pin, peer_id, r->ltk);
            rc = AFROS_SUCCESS;
        } else {
            rc = AFROS_ERROR;
        }
        g_trust.pending.active = false;
    }
    pthread_mutex_unlock(&g_trust.lock);
    return rc;
}

/**
 * @brief Check whether a peer device is paired and trusted.
 * @return 1 if paired, 0 if not, -AFROS_ERROR_INVALID_PARAM on bad args.
 */
int32_t TrustCheckPaired(const char *peer_id)
{
    if (peer_id == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_trust.lock);
    afros_trust_record_t *r = find_locked(peer_id);
    int32_t rc = (r != NULL && r->state == AFROS_PAIR_CONFIRMED) ? 1 : 0;
    if (rc) {
        r->last_used_ms = now_ms();
    }
    pthread_mutex_unlock(&g_trust.lock);
    return rc;
}

/** @brief Fetch the long-term key for a paired device. */
int32_t TrustGetLtk(const char *peer_id, uint8_t *out_key, uint32_t cap)
{
    if (peer_id == NULL || out_key == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    if (cap < AFROS_TRUST_KEY_LEN) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_trust.lock);
    afros_trust_record_t *r = find_locked(peer_id);
    int32_t rc;
    if (r == NULL || r->state != AFROS_PAIR_CONFIRMED) {
        rc = AFROS_ERROR;
    } else {
        memcpy(out_key, r->ltk, AFROS_TRUST_KEY_LEN);
        rc = AFROS_SUCCESS;
    }
    pthread_mutex_unlock(&g_trust.lock);
    return rc;
}

/**
 * @brief List paired device ids.
 * @param out    Output buffer (may be NULL when *count_inout==0).
 * @param count_inout  In: capacity; Out: number of ids written.
 */
int32_t TrustList(char (*out)[64], uint32_t *count_inout)
{
    if (count_inout == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_trust.lock);
    uint32_t cap = *count_inout, n = 0;
    for (uint32_t i = 0; i < AFROS_TRUST_MAX_DEVICES && n < cap; ++i) {
        if (g_trust.records[i].valid &&
            g_trust.records[i].state == AFROS_PAIR_CONFIRMED) {
            if (out != NULL) {
                strncpy(out[n], g_trust.records[i].device_id, 63);
                out[n][63] = '\0';
            }
            n++;
        }
    }
    *count_inout = n;
    pthread_mutex_unlock(&g_trust.lock);
    return AFROS_SUCCESS;
}

/** @brief Forget a paired device. */
int32_t TrustUnpair(const char *peer_id)
{
    if (peer_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_trust.lock);
    afros_trust_record_t *r = find_locked(peer_id);
    if (r != NULL) {
        memset(r, 0, sizeof(*r));
        if (g_trust.count > 0) {
            g_trust.count--;
        }
    }
    pthread_mutex_unlock(&g_trust.lock);
    return AFROS_SUCCESS;
}
