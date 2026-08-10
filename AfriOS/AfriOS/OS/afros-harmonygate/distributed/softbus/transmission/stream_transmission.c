/**
 * @file stream_transmission.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus low-latency stream.
 *
 * A simple sliding-window stream protocol: each datagram carries a 4-byte
 * sequence number, a 1-byte frame type, and up to 1400 bytes of payload.
 * Used for camera share / sensor share where throughput > reliability.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define AFROS_ST_MAX_STREAMS  8
#define AFROS_ST_MAX_PAYLOAD  1400
#define AFROS_ST_HEADER_BYTES 5
#define AFROS_ST_WINDOW       16

/** Frame types carried over the stream. */
typedef enum {
    AFROS_ST_FRAME_DATA    = 0,
    AFROS_ST_FRAME_ACK     = 1,
    AFROS_ST_FRAME_HEARTBEAT = 2,
    AFROS_ST_FRAME_CLOSE   = 3,
} afros_st_frame_type_t;

/** SoftBus send callback. */
typedef int32_t (*afros_st_send_fn)(int32_t id, const uint8_t *buf, uint32_t len);
/** SoftBus recv callback. */
typedef int32_t (*afros_st_recv_fn)(int32_t id, uint8_t *buf, uint32_t cap);

typedef struct {
    int32_t      conn_id;
    uint32_t     tx_seq;        /**< Next sequence number to send. */
    uint32_t     rx_seq;        /**< Next expected receive sequence. */
    afros_st_send_fn send_fn;
    afros_st_recv_fn recv_fn;
    bool         in_use;
    pthread_mutex_t lock;
} afros_st_stream_t;

static afros_st_stream_t g_st[AFROS_ST_MAX_STREAMS];

static afros_st_stream_t *find_free(void)
{
    for (uint32_t i = 0; i < AFROS_ST_MAX_STREAMS; ++i) {
        if (!g_st[i].in_use) {
            memset(&g_st[i], 0, sizeof(g_st[i]));
            g_st[i].in_use = true;
            pthread_mutex_init(&g_st[i].lock, NULL);
            return &g_st[i];
        }
    }
    return NULL;
}

static afros_st_stream_t *get(int32_t sid)
{
    if (sid < 0 || sid >= AFROS_ST_MAX_STREAMS) {
        return NULL;
    }
    if (!g_st[sid].in_use) {
        return NULL;
    }
    return &g_st[sid];
}

/**
 * @brief Open a stream over an existing SoftBus connection.
 */
int32_t StreamOpen(int32_t conn_id, afros_st_send_fn send_fn,
                                  afros_st_recv_fn recv_fn)
{
    if (send_fn == NULL || recv_fn == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_st_stream_t *s = find_free();
    if (s == NULL) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    s->conn_id = conn_id;
    s->send_fn = send_fn;
    s->recv_fn = recv_fn;
    s->tx_seq  = 1;
    s->rx_seq  = 1;
    return (int32_t)(s - g_st);
}

/** Pack a stream frame header. */
static uint32_t pack_hdr(uint8_t *buf, uint32_t seq, afros_st_frame_type_t t)
{
    buf[0] = (uint8_t)(seq & 0xFF);
    buf[1] = (uint8_t)((seq >> 8) & 0xFF);
    buf[2] = (uint8_t)((seq >> 16) & 0xFF);
    buf[3] = (uint8_t)((seq >> 24) & 0xFF);
    buf[4] = (uint8_t)t;
    return AFROS_ST_HEADER_BYTES;
}

/** Unpack a stream frame header. */
static void unpack_hdr(const uint8_t *buf, uint32_t *seq,
                       afros_st_frame_type_t *t)
{
    *seq = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    *t   = (afros_st_frame_type_t)buf[4];
}

/**
 * @brief Send a payload datagram on the stream.
 * @return Bytes of payload sent, or -AFROS_ERROR_* on failure.
 */
int32_t StreamSend(int32_t sid, const uint8_t *payload, uint32_t len)
{
    if (payload == NULL && len > 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    if (len > AFROS_ST_MAX_PAYLOAD) {
        len = AFROS_ST_MAX_PAYLOAD;
    }
    afros_st_stream_t *s = get(sid);
    if (s == NULL) {
        return -AFROS_ERROR;
    }
    static uint8_t frame[AFROS_ST_HEADER_BYTES + AFROS_ST_MAX_PAYLOAD];
    pthread_mutex_lock(&s->lock);
    pack_hdr(frame, s->tx_seq, AFROS_ST_FRAME_DATA);
    if (len > 0) {
        memcpy(frame + AFROS_ST_HEADER_BYTES, payload, len);
    }
    int32_t sent = s->send_fn(s->conn_id, frame,
                              AFROS_ST_HEADER_BYTES + len);
    if (sent > 0) {
        s->tx_seq++;
    }
    pthread_mutex_unlock(&s->lock);
    return (sent > 0) ? (int32_t)len : -AFROS_ERROR;
}

/**
 * @brief Receive the next payload datagram from the stream.
 * @return Bytes of payload received, or -AFROS_ERROR_* on failure.
 */
int32_t StreamRecv(int32_t sid, uint8_t *out, uint32_t cap)
{
    if (out == NULL || cap == 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_st_stream_t *s = get(sid);
    if (s == NULL) {
        return -AFROS_ERROR;
    }
    static uint8_t frame[AFROS_ST_HEADER_BYTES + AFROS_ST_MAX_PAYLOAD];
    int32_t n = s->recv_fn(s->conn_id, frame, sizeof(frame));
    if (n < (int32_t)AFROS_ST_HEADER_BYTES) {
        return -AFROS_ERROR_TIMEOUT;
    }
    uint32_t seq;
    afros_st_frame_type_t t;
    unpack_hdr(frame, &seq, &t);
    if (t == AFROS_ST_FRAME_CLOSE) {
        return 0;
    }
    if (t != AFROS_ST_FRAME_DATA) {
        /* Heartbeats / ACKs are silently dropped in the sandbox. */
        return 0;
    }
    pthread_mutex_lock(&s->lock);
    s->rx_seq = seq + 1;
    pthread_mutex_unlock(&s->lock);
    uint32_t plen = (uint32_t)n - AFROS_ST_HEADER_BYTES;
    if (plen > cap) {
        plen = cap;
    }
    memcpy(out, frame + AFROS_ST_HEADER_BYTES, plen);
    return (int32_t)plen;
}

/**
 * @brief Send a heartbeat to keep the stream alive.
 */
int32_t StreamHeartbeat(int32_t sid)
{
    afros_st_stream_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    uint8_t frame[AFROS_ST_HEADER_BYTES];
    pthread_mutex_lock(&s->lock);
    pack_hdr(frame, s->tx_seq, AFROS_ST_FRAME_HEARTBEAT);
    s->send_fn(s->conn_id, frame, sizeof(frame));
    pthread_mutex_unlock(&s->lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Close a stream. Sends a CLOSE frame to the peer then frees the slot.
 */
int32_t StreamClose(int32_t sid)
{
    afros_st_stream_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    uint8_t frame[AFROS_ST_HEADER_BYTES];
    pack_hdr(frame, 0, AFROS_ST_FRAME_CLOSE);
    s->send_fn(s->conn_id, frame, sizeof(frame));
    pthread_mutex_destroy(&s->lock);
    memset(s, 0, sizeof(*s));
    return AFROS_SUCCESS;
}

/** @brief Query the next outgoing sequence number (diagnostics). */
uint32_t StreamTxSeq(int32_t sid)
{
    afros_st_stream_t *s = get(sid);
    if (s == NULL) {
        return 0;
    }
    return s->tx_seq;
}
