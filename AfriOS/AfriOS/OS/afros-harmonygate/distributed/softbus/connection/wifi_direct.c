/**
 * @file wifi_direct.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus Wi-Fi Direct (Wi-Fi P2P) transport.
 *
 * Wi-Fi Direct forms a P2P group: one device is the Group Owner (GO) and acts
 * as a soft-AP, the others are clients. We expose a connection-oriented API
 * mirroring the BT and TCP transports, backed by the same in-memory loop.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define AFROS_WFD_MAX_CONN  4
#define AFROS_WFD_BUF_BYTES 8192
#define AFROS_WFD_MAC_LEN   18

typedef struct {
    uint8_t  buf[AFROS_WFD_BUF_BYTES];
    uint32_t len;
    char     peer_mac[AFROS_WFD_MAC_LEN];
    bool     is_group_owner;
    bool     in_use;
    pthread_mutex_t lock;
} afros_wfd_conn_t;

static afros_wfd_conn_t g_wfd[AFROS_WFD_MAX_CONN];

static afros_wfd_conn_t *find_free(void)
{
    for (uint32_t i = 0; i < AFROS_WFD_MAX_CONN; ++i) {
        if (!g_wfd[i].in_use) {
            memset(&g_wfd[i], 0, sizeof(g_wfd[i]));
            g_wfd[i].in_use = true;
            pthread_mutex_init(&g_wfd[i].lock, NULL);
            return &g_wfd[i];
        }
    }
    return NULL;
}

static afros_wfd_conn_t *get(int32_t id)
{
    if (id < 0 || id >= AFROS_WFD_MAX_CONN) {
        return NULL;
    }
    if (!g_wfd[id].in_use) {
        return NULL;
    }
    return &g_wfd[id];
}

/**
 * @brief Form or join a Wi-Fi Direct group with a peer.
 * @param peer_mac        Peer MAC address ("AA:BB:CC:DD:EE:FF").
 * @param become_go       True if we should negotiate to be Group Owner.
 * @return Connection id ≥ 0, or -AFROS_ERROR_* on failure.
 */
int32_t WifiDirectConnect(const char *peer_mac, bool become_go)
{
    if (peer_mac == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_wfd_conn_t *c = find_free();
    if (c == NULL) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    strncpy(c->peer_mac, peer_mac, AFROS_WFD_MAC_LEN - 1);
    c->is_group_owner = become_go;
    return (int32_t)(c - g_wfd);
}

/**
 * @brief Send up to @p len bytes over the Wi-Fi Direct link.
 */
int32_t WifiDirectSend(int32_t id, const uint8_t *buf, uint32_t len)
{
    if (buf == NULL && len > 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_wfd_conn_t *c = get(id);
    if (c == NULL) {
        return -AFROS_ERROR;
    }
    if (len > AFROS_WFD_BUF_BYTES) {
        len = AFROS_WFD_BUF_BYTES;
    }
    pthread_mutex_lock(&c->lock);
    memcpy(c->buf, buf, len);
    c->len = len;
    pthread_mutex_unlock(&c->lock);
    return (int32_t)len;
}

/**
 * @brief Receive up to @p cap bytes from the Wi-Fi Direct link.
 */
int32_t WifiDirectRecv(int32_t id, uint8_t *buf, uint32_t cap)
{
    if (buf == NULL || cap == 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_wfd_conn_t *c = get(id);
    if (c == NULL) {
        return -AFROS_ERROR;
    }
    pthread_mutex_lock(&c->lock);
    uint32_t n = (c->len < cap) ? c->len : cap;
    memcpy(buf, c->buf, n);
    if (n < c->len) {
        memmove(c->buf, c->buf + n, c->len - n);
        c->len -= n;
    } else {
        c->len = 0;
    }
    pthread_mutex_unlock(&c->lock);
    return (int32_t)n;
}

/** @brief Close the Wi-Fi Direct connection. */
int32_t WifiDirectClose(int32_t id)
{
    afros_wfd_conn_t *c = get(id);
    if (c == NULL) {
        return AFROS_ERROR;
    }
    pthread_mutex_destroy(&c->lock);
    memset(c, 0, sizeof(*c));
    return AFROS_SUCCESS;
}

/** @brief True if this side is the Group Owner of the P2P group. */
int32_t WifiDirectIsGroupOwner(int32_t id, bool *out)
{
    if (out == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    afros_wfd_conn_t *c = get(id);
    if (c == NULL) {
        return AFROS_ERROR;
    }
    *out = c->is_group_owner;
    return AFROS_SUCCESS;
}

/** @brief Query the peer MAC of a connection. */
int32_t WifiDirectGetPeer(int32_t id, char *out, uint32_t cap)
{
    if (out == NULL || cap == 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    afros_wfd_conn_t *c = get(id);
    if (c == NULL) {
        return AFROS_ERROR;
    }
    strncpy(out, c->peer_mac, cap - 1);
    out[cap - 1] = '\0';
    return AFROS_SUCCESS;
}
