/**
 * @file device_auth.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus device authentication.
 *
 * Implements an HMAC-SHA256 challenge-response handshake. The initiator
 * picks a random nonce, the responder answers with HMAC(ltk, nonce||peer).
 * On success both sides derive a session key (HKDF-ish expansion of the
 * shared ltk + both nonces) used by file_transmission / stream_transmission.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define AFROS_AUTH_NONCE_LEN  16
#define AFROS_AUTH_HMAC_LEN   32
#define AFROS_AUTH_KEY_LEN    32
#define AFROS_AUTH_MAX_SESSIONS 16

/** One in-flight or established authentication session. */
typedef struct {
    char     peer_id[64];
    uint8_t  local_nonce[AFROS_AUTH_NONCE_LEN];
    uint8_t  peer_nonce[AFROS_AUTH_NONCE_LEN];
    uint8_t  session_key[AFROS_AUTH_KEY_LEN];
    bool     established;
    bool     in_use;
    uint64_t created_ms;
} afros_auth_session_t;

static struct {
    afros_auth_session_t sessions[AFROS_AUTH_MAX_SESSIONS];
    pthread_mutex_t      lock;
} g_auth = { .lock = PTHREAD_MUTEX_INITIALIZER };

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

/** Fill 16 bytes with pseudo-random data (sandbox: rand()). */
static void fill_random(uint8_t *out, uint32_t len)
{
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = true;
    }
    for (uint32_t i = 0; i < len; ++i) {
        out[i] = (uint8_t)(rand() & 0xFF);
    }
}

/** Trivial HMAC-SHA256 stand-in: keyed FNV-1a folded over the message. */
static void hmac_stub(const uint8_t *key, uint32_t key_len,
                      const uint8_t *msg, uint32_t msg_len,
                      uint8_t *out)
{
    uint8_t kpad[64];
    memset(kpad, 0, sizeof(kpad));
    if (key_len > sizeof(kpad)) {
        key_len = sizeof(kpad);
    }
    memcpy(kpad, key, key_len);
    for (uint32_t i = 0; i < sizeof(kpad); ++i) {
        kpad[i] ^= 0x36;
    }
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < sizeof(kpad); ++i) {
        h ^= kpad[i]; h *= 16777619u;
    }
    for (uint32_t i = 0; i < msg_len; ++i) {
        h ^= msg[i]; h *= 16777619u;
    }
    uint8_t inner[4];
    inner[0] = (uint8_t)(h & 0xFF);
    inner[1] = (uint8_t)((h >> 8) & 0xFF);
    inner[2] = (uint8_t)((h >> 16) & 0xFF);
    inner[3] = (uint8_t)((h >> 24) & 0xFF);

    for (uint32_t i = 0; i < sizeof(kpad); ++i) {
        kpad[i] ^= (0x36 ^ 0x5C); /* undo 0x36, apply 0x5C */
    }
    h = 2166136261u;
    for (uint32_t i = 0; i < sizeof(kpad); ++i) {
        h ^= kpad[i]; h *= 16777619u;
    }
    for (uint32_t i = 0; i < sizeof(inner); ++i) {
        h ^= inner[i]; h *= 16777619u;
    }
    for (uint32_t i = 0; i < AFROS_AUTH_HMAC_LEN; ++i) {
        h ^= h >> 13; h *= 0x85ebca6bu;
        out[i] = (uint8_t)(h >> 24);
    }
}

static afros_auth_session_t *find_or_alloc_locked(const char *peer_id)
{
    for (uint32_t i = 0; i < AFROS_AUTH_MAX_SESSIONS; ++i) {
        if (g_auth.sessions[i].in_use &&
            strncmp(g_auth.sessions[i].peer_id, peer_id, 63) == 0) {
            return &g_auth.sessions[i];
        }
    }
    for (uint32_t i = 0; i < AFROS_AUTH_MAX_SESSIONS; ++i) {
        if (!g_auth.sessions[i].in_use) {
            memset(&g_auth.sessions[i], 0, sizeof(g_auth.sessions[i]));
            strncpy(g_auth.sessions[i].peer_id, peer_id, 63);
            g_auth.sessions[i].in_use    = true;
            g_auth.sessions[i].created_ms = now_ms();
            return &g_auth.sessions[i];
        }
    }
    return NULL;
}

/**
 * @brief Start an authentication session: generate a local nonce to send.
 * @param peer_id     Peer device identifier.
 * @param out_nonce   Receives the local nonce (16 bytes).
 * @return AFROS_SUCCESS or AFROS_ERROR_NO_MEMORY.
 */
int32_t DeviceAuthStart(const char *peer_id, uint8_t *out_nonce)
{
    if (peer_id == NULL || out_nonce == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_auth.lock);
    afros_auth_session_t *s = find_or_alloc_locked(peer_id);
    if (s == NULL) {
        pthread_mutex_unlock(&g_auth.lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    fill_random(s->local_nonce, AFROS_AUTH_NONCE_LEN);
    memcpy(out_nonce, s->local_nonce, AFROS_AUTH_NONCE_LEN);
    s->established = false;
    pthread_mutex_unlock(&g_auth.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Verify the peer's HMAC challenge-response and derive a session key.
 * @param peer_id        Peer device identifier.
 * @param ltk            Long-term key (from TrustManager).
 * @param ltk_len        LTK length in bytes.
 * @param peer_nonce     Nonce received from the peer.
 * @param peer_hmac      HMAC the peer sent back.
 * @return AFROS_SUCCESS if verified, AFROS_ERROR otherwise.
 */
int32_t DeviceAuthVerify(const char    *peer_id,
                         const uint8_t *ltk,
                         uint32_t       ltk_len,
                         const uint8_t *peer_nonce,
                         const uint8_t *peer_hmac)
{
    if (peer_id == NULL || ltk == NULL || peer_nonce == NULL ||
        peer_hmac == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_auth.lock);
    afros_auth_session_t *s = find_or_alloc_locked(peer_id);
    if (s == NULL) {
        pthread_mutex_unlock(&g_auth.lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    /* Compute expected HMAC over (local_nonce || peer_nonce). */
    uint8_t msg[AFROS_AUTH_NONCE_LEN * 2];
    memcpy(msg, s->local_nonce, AFROS_AUTH_NONCE_LEN);
    memcpy(msg + AFROS_AUTH_NONCE_LEN, peer_nonce, AFROS_AUTH_NONCE_LEN);
    uint8_t expected[AFROS_AUTH_HMAC_LEN];
    hmac_stub(ltk, ltk_len, msg, sizeof(msg), expected);
    if (memcmp(expected, peer_hmac, AFROS_AUTH_HMAC_LEN) != 0) {
        pthread_mutex_unlock(&g_auth.lock);
        return AFROS_ERROR;
    }
    /* Verified: stash peer nonce and derive the session key. */
    memcpy(s->peer_nonce, peer_nonce, AFROS_AUTH_NONCE_LEN);
    uint8_t derive_input[AFROS_AUTH_NONCE_LEN * 2];
    memcpy(derive_input, s->local_nonce, AFROS_AUTH_NONCE_LEN);
    memcpy(derive_input + AFROS_AUTH_NONCE_LEN, s->peer_nonce, AFROS_AUTH_NONCE_LEN);
    hmac_stub(ltk, ltk_len, derive_input, sizeof(derive_input),
              s->session_key);
    s->established = true;
    pthread_mutex_unlock(&g_auth.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Fetch the session key for an established session.
 */
int32_t DeviceAuthSessionKey(const char *peer_id,
                             uint8_t    *out_key,
                             uint32_t    cap)
{
    if (peer_id == NULL || out_key == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    if (cap < AFROS_AUTH_KEY_LEN) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_auth.lock);
    afros_auth_session_t *s = find_or_alloc_locked(peer_id);
    int32_t rc;
    if (s == NULL || !s->established) {
        rc = AFROS_ERROR;
    } else {
        memcpy(out_key, s->session_key, AFROS_AUTH_KEY_LEN);
        rc = AFROS_SUCCESS;
    }
    pthread_mutex_unlock(&g_auth.lock);
    return rc;
}

/**
 * @brief Compute the local response HMAC for a challenge received from a peer.
 *        Used when we are the responder side of the handshake.
 */
int32_t DeviceAuthComputeResponse(const char    *peer_id,
                                  const uint8_t *ltk,
                                  uint32_t       ltk_len,
                                  const uint8_t *peer_nonce,
                                  uint8_t       *out_hmac)
{
    if (peer_id == NULL || ltk == NULL || peer_nonce == NULL || out_hmac == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_auth.lock);
    afros_auth_session_t *s = find_or_alloc_locked(peer_id);
    if (s == NULL) {
        pthread_mutex_unlock(&g_auth.lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    if (!s->in_use || s->local_nonce[0] == 0) {
        /* Responder side: ensure we have a local nonce. */
        fill_random(s->local_nonce, AFROS_AUTH_NONCE_LEN);
    }
    memcpy(s->peer_nonce, peer_nonce, AFROS_AUTH_NONCE_LEN);
    uint8_t msg[AFROS_AUTH_NONCE_LEN * 2];
    memcpy(msg, peer_nonce, AFROS_AUTH_NONCE_LEN);
    memcpy(msg + AFROS_AUTH_NONCE_LEN, s->local_nonce, AFROS_AUTH_NONCE_LEN);
    hmac_stub(ltk, ltk_len, msg, sizeof(msg), out_hmac);
    pthread_mutex_unlock(&g_auth.lock);
    return AFROS_SUCCESS;
}

/** @brief Tear down a session. */
int32_t DeviceAuthClose(const char *peer_id)
{
    if (peer_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_auth.lock);
    for (uint32_t i = 0; i < AFROS_AUTH_MAX_SESSIONS; ++i) {
        if (g_auth.sessions[i].in_use &&
            strncmp(g_auth.sessions[i].peer_id, peer_id, 63) == 0) {
            memset(&g_auth.sessions[i], 0, sizeof(g_auth.sessions[i]));
            break;
        }
    }
    pthread_mutex_unlock(&g_auth.lock);
    return AFROS_SUCCESS;
}
