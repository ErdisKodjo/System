/**
 * @file file_transmission.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus chunked file transfer.
 *
 * Splits a file into fixed-size chunks, frames each chunk with a small
 * header (chunk_id, total_chunks, sha256 truncated checksum), and ships it
 * over a SoftBus connection. Supports resume: the receiver tells the sender
 * the next expected chunk id.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define AFROS_FT_CHUNK_BYTES   4096
#define AFROS_FT_HEADER_BYTES  12
#define AFROS_FT_CHECKSUM_LEN  4
#define AFROS_FT_MAX_CHUNKS    65536

/** On-wire frame layout: [chunk_id(4)][total(4)][checksum(4)][payload(N)]. */
typedef struct {
    uint32_t chunk_id;
    uint32_t total_chunks;
    uint8_t  checksum[AFROS_FT_CHECKSUM_LEN];
} afros_ft_header_t;

/** SoftBus send callback (matches TcpSend / BtSend / WifiDirectSend). */
typedef int32_t (*afros_ft_send_fn)(int32_t id, const uint8_t *buf, uint32_t len);
/** SoftBus recv callback. */
typedef int32_t (*afros_ft_recv_fn)(int32_t id, uint8_t *buf, uint32_t cap);

/** Truncated SHA-256 stand-in: FNV-1a over the chunk → 4 bytes. */
static void checksum(const uint8_t *buf, uint32_t len, uint8_t *out)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < len; ++i) {
        h ^= buf[i];
        h *= 16777619u;
    }
    out[0] = (uint8_t)(h & 0xFF);
    out[1] = (uint8_t)((h >> 8) & 0xFF);
    out[2] = (uint8_t)((h >> 16) & 0xFF);
    out[3] = (uint8_t)((h >> 24) & 0xFF);
}

/** Write header + payload into a single buffer for send(). */
static int32_t pack_frame(const afros_ft_header_t *h,
                          const uint8_t *payload, uint32_t payload_len,
                          uint8_t *out, uint32_t cap)
{
    if (cap < AFROS_FT_HEADER_BYTES + payload_len) {
        return AFROS_ERROR_NO_MEMORY;
    }
    out[0] = (uint8_t)(h->chunk_id & 0xFF);
    out[1] = (uint8_t)((h->chunk_id >> 8) & 0xFF);
    out[2] = (uint8_t)((h->chunk_id >> 16) & 0xFF);
    out[3] = (uint8_t)((h->chunk_id >> 24) & 0xFF);
    out[4] = (uint8_t)(h->total_chunks & 0xFF);
    out[5] = (uint8_t)((h->total_chunks >> 8) & 0xFF);
    out[6] = (uint8_t)((h->total_chunks >> 16) & 0xFF);
    out[7] = (uint8_t)((h->total_chunks >> 24) & 0xFF);
    memcpy(out + 8, h->checksum, AFROS_FT_CHECKSUM_LEN);
    if (payload_len > 0) {
        memcpy(out + AFROS_FT_HEADER_BYTES, payload, payload_len);
    }
    return (int32_t)(AFROS_FT_HEADER_BYTES + payload_len);
}

/**
 * @brief Send a file over a SoftBus connection.
 * @param conn_id    Connection id (TCP / BT / Wi-Fi Direct).
 * @param data       File contents.
 * @param data_len   File length in bytes.
 * @param send_fn    SoftBus send callback.
 * @param resume_at  Chunk id to resume from (0 for fresh transfer).
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t FileTransferSend(int32_t conn_id,
                         const uint8_t *data,
                         uint32_t       data_len,
                         afros_ft_send_fn send_fn,
                         uint32_t       resume_at)
{
    if (data == NULL && data_len > 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    if (send_fn == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    uint32_t total = (data_len + AFROS_FT_CHUNK_BYTES - 1) / AFROS_FT_CHUNK_BYTES;
    if (total == 0) {
        total = 1;
    }
    if (total > AFROS_FT_MAX_CHUNKS) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    static uint8_t frame[AFROS_FT_HEADER_BYTES + AFROS_FT_CHUNK_BYTES];
    for (uint32_t i = resume_at; i < total; ++i) {
        uint32_t off  = i * AFROS_FT_CHUNK_BYTES;
        uint32_t plen = AFROS_FT_CHUNK_BYTES;
        if (off + plen > data_len) {
            plen = data_len - off;
        }
        afros_ft_header_t h;
        h.chunk_id     = i;
        h.total_chunks = total;
        checksum(data + off, plen, h.checksum);
        int32_t n = pack_frame(&h, data + off, plen, frame, sizeof(frame));
        if (n < 0) {
            return -n;
        }
        int32_t sent = send_fn(conn_id, frame, (uint32_t)n);
        if (sent != n) {
            return AFROS_ERROR_TIMEOUT;
        }
    }
    return AFROS_SUCCESS;
}

/**
 * @brief Receive a file over a SoftBus connection.
 * @param conn_id   Connection id.
 * @param recv_fn   SoftBus recv callback.
 * @param out       Caller-allocated buffer (must be big enough).
 * @param out_cap   Capacity of @p out.
 * @param out_len   Receives the number of bytes written.
 * @return AFROS_SUCCESS, AFROS_ERROR_NO_MEMORY on capacity, or AFROS_ERROR
 *         on a checksum mismatch.
 */
int32_t FileTransferRecv(int32_t          conn_id,
                         afros_ft_recv_fn recv_fn,
                         uint8_t         *out,
                         uint32_t         out_cap,
                         uint32_t        *out_len)
{
    if (recv_fn == NULL || out == NULL || out_len == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    *out_len = 0;
    static uint8_t frame[AFROS_FT_HEADER_BYTES + AFROS_FT_CHUNK_BYTES];
    uint32_t received = 0;
    uint32_t expected = 0;
    for (;;) {
        int32_t n = recv_fn(conn_id, frame, sizeof(frame));
        if (n <= 0) {
            break;
        }
        if (n < AFROS_FT_HEADER_BYTES) {
            return AFROS_ERROR;
        }
        afros_ft_header_t h;
        h.chunk_id     = (uint32_t)frame[0] | ((uint32_t)frame[1] << 8) |
                         ((uint32_t)frame[2] << 16) | ((uint32_t)frame[3] << 24);
        h.total_chunks = (uint32_t)frame[4] | ((uint32_t)frame[5] << 8) |
                         ((uint32_t)frame[6] << 16) | ((uint32_t)frame[7] << 24);
        memcpy(h.checksum, frame + 8, AFROS_FT_CHECKSUM_LEN);
        uint32_t plen = (uint32_t)n - AFROS_FT_HEADER_BYTES;
        uint8_t cs[AFROS_FT_CHECKSUM_LEN];
        checksum(frame + AFROS_FT_HEADER_BYTES, plen, cs);
        if (memcmp(cs, h.checksum, AFROS_FT_CHECKSUM_LEN) != 0) {
            return AFROS_ERROR; /* Corrupt chunk. */
        }
        if (expected == 0) {
            expected = h.total_chunks;
        }
        if (received + plen > out_cap) {
            return AFROS_ERROR_NO_MEMORY;
        }
        memcpy(out + received, frame + AFROS_FT_HEADER_BYTES, plen);
        received += plen;
        if (h.chunk_id + 1 >= expected) {
            break;
        }
    }
    *out_len = received;
    return AFROS_SUCCESS;
}

/** @brief Compute the number of chunks a file will need. */
uint32_t FileTransferChunkCount(uint32_t data_len)
{
    uint32_t n = (data_len + AFROS_FT_CHUNK_BYTES - 1) / AFROS_FT_CHUNK_BYTES;
    return (n == 0) ? 1 : n;
}
