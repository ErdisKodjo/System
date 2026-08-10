/**
 * @file bluetooth_connection.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus Bluetooth transport.
 *
 * The sandbox has no BT controller, so this file emulates an RFCOMM-style
 * stream by holding the most recently sent buffer in a slot and returning
 * it to the receiver. This is enough to exercise the file/stream
 * transmission protocols above.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define AFROS_BT_MAX_CONN    4
#define AFROS_BT_BUF_BYTES   4096
#define AFROS_BT_ADDR_LEN    18  /* "AA:BB:CC:DD:EE:FF\0". */

typedef struct {
    uint8_t  buf[AFROS_BT_BUF_BYTES];
    uint32_t len;
    char     peer_addr[AFROS_BT_ADDR_LEN];
    bool     in_use;
    pthread_mutex_t lock;
} afros_bt_conn_t;

static afros_bt_conn_t g_bt[AFROS_BT_MAX_CONN];

static afros_bt_conn_t *find_free(void)
{
    for (uint32_t i = 0; i < AFROS_BT_MAX_CONN; ++i) {
        if (!g_bt[i].in_use) {
            memset(&g_bt[i], 0, sizeof(g_bt[i]));
            g_bt[i].in_use = true;
            pthread_mutex_init(&g_bt[i].lock, NULL);
            return &g_bt[i];
        }
    }
    return NULL;
}

static afros_bt_conn_t *get(int32_t id)
{
    if (id < 0 || id >= AFROS_BT_MAX_CONN) {
        return NULL;
    }
    if (!g_bt[id].in_use) {
        return NULL;
    }
    return &g_bt[id];
}

/**
 * @brief Open a Bluetooth RFCOMM channel to a peer.
 * @param addr     Peer BT address (e.g. "AA:BB:CC:DD:EE:FF").
 * @param channel  RFCOMM channel (1-30).
 * @return Connection id ≥ 0, or -AFROS_ERROR_* on failure.
 */
int32_t BtConnect(const char *addr, uint8_t channel)
{
    if (addr == NULL || channel == 0 || channel > 30) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_bt_conn_t *c = find_free();
    if (c == NULL) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    strncpy(c->peer_addr, addr, AFROS_BT_ADDR_LEN - 1);
    return (int32_t)(c - g_bt);
}

/**
 * @brief Send up to @p len bytes on a Bluetooth connection.
 * @return Bytes sent, or -AFROS_ERROR_* on failure.
 */
int32_t BtSend(int32_t id, const uint8_t *buf, uint32_t len)
{
    if (buf == NULL && len > 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_bt_conn_t *c = get(id);
    if (c == NULL) {
        return -AFROS_ERROR;
    }
    if (len > AFROS_BT_BUF_BYTES) {
        len = AFROS_BT_BUF_BYTES;
    }
    pthread_mutex_lock(&c->lock);
    memcpy(c->buf, buf, len);
    c->len = len;
    pthread_mutex_unlock(&c->lock);
    return (int32_t)len;
}

/**
 * @brief Receive up to @p cap bytes on a Bluetooth connection.
 *        Returns whatever was last sent on that connection.
 */
int32_t BtRecv(int32_t id, uint8_t *buf, uint32_t cap)
{
    if (buf == NULL || cap == 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_bt_conn_t *c = get(id);
    if (c == NULL) {
        return -AFROS_ERROR;
    }
    pthread_mutex_lock(&c->lock);
    uint32_t n = (c->len < cap) ? c->len : cap;
    memcpy(buf, c->buf, n);
    /* Consume the bytes from the sandbox buffer. */
    if (n < c->len) {
        memmove(c->buf, c->buf + n, c->len - n);
        c->len -= n;
    } else {
        c->len = 0;
    }
    pthread_mutex_unlock(&c->lock);
    return (int32_t)n;
}

/**
 * @brief Close a Bluetooth connection and release its slot.
 */
int32_t BtClose(int32_t id)
{
    afros_bt_conn_t *c = get(id);
    if (c == NULL) {
        return AFROS_ERROR;
    }
    pthread_mutex_destroy(&c->lock);
    memset(c, 0, sizeof(*c));
    return AFROS_SUCCESS;
}

/** @brief Query the peer BT address of a connection. */
int32_t BtGetPeer(int32_t id, char *out, uint32_t cap)
{
    if (out == NULL || cap == 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    afros_bt_conn_t *c = get(id);
    if (c == NULL) {
        return AFROS_ERROR;
    }
    strncpy(out, c->peer_addr, cap - 1);
    out[cap - 1] = '\0';
    return AFROS_SUCCESS;
}

/** @brief Number of bytes currently buffered on the connection. */
uint32_t BtPendingBytes(int32_t id)
{
    afros_bt_conn_t *c = get(id);
    if (c == NULL) {
        return 0;
    }
    pthread_mutex_lock(&c->lock);
    uint32_t n = c->len;
    pthread_mutex_unlock(&c->lock);
    return n;
}
