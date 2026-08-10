/**
 * @file storage_sharing.c
 * @brief AfriOS HarmonyOS compatibility — distributed storage sharing.
 *
 * Exposes a local directory to a remote device over SoftBus. Supports
 * read-only and read-write modes. Operations are routed through the
 * SoftBus stream as small JSON-ish request/response frames.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define AFROS_SS_MAX_SESSIONS  4
#define AFROS_SS_PATH_LEN      256
#define AFROS_SS_CHUNK_BYTES   4096
#define AFROS_SS_META_BYTES    16

typedef enum {
    AFROS_SS_MODE_READ_ONLY = 0,
    AFROS_SS_MODE_READ_WRITE = 1,
} afros_ss_mode_t;

typedef struct {
    int32_t         stream_id;
    afros_ss_mode_t mode;
    char            root[AFROS_SS_PATH_LEN];
    bool            in_use;
    pthread_mutex_t lock;
    uint64_t        bytes_read;
    uint64_t        bytes_written;
} afros_ss_session_t;

static afros_ss_session_t g_ss[AFROS_SS_MAX_SESSIONS];

static afros_ss_session_t *find_free(void)
{
    for (uint32_t i = 0; i < AFROS_SS_MAX_SESSIONS; ++i) {
        if (!g_ss[i].in_use) {
            memset(&g_ss[i], 0, sizeof(g_ss[i]));
            g_ss[i].in_use = true;
            pthread_mutex_init(&g_ss[i].lock, NULL);
            return &g_ss[i];
        }
    }
    return NULL;
}

static afros_ss_session_t *get(int32_t sid)
{
    if (sid < 0 || sid >= AFROS_SS_MAX_SESSIONS) {
        return NULL;
    }
    if (!g_ss[sid].in_use) {
        return NULL;
    }
    return &g_ss[sid];
}

/** Resolve a remote-relative path against the local root (basic sandboxing). */
static int32_t resolve_path(afros_ss_session_t *s,
                            const char *remote_path,
                            char *out, uint32_t cap)
{
    if (remote_path == NULL || remote_path[0] == '/' ||
        strstr(remote_path, "..") != NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    int n = snprintf(out, cap, "%s/%s", s->root, remote_path);
    if (n <= 0 || (uint32_t)n >= cap) {
        return AFROS_ERROR_NO_MEMORY;
    }
    return AFROS_SUCCESS;
}

/**
 * @brief Start sharing a directory with a peer.
 * @param root       Absolute local path to share.
 * @param stream_id  SoftBus stream id.
 * @param mode       Read-only or read-write.
 * @return Session id ≥ 0, or -AFROS_ERROR_* on failure.
 */
int32_t StorageShareStart(const char *root, int32_t stream_id,
                          afros_ss_mode_t mode)
{
    if (root == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_ss_session_t *s = find_free();
    if (s == NULL) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    strncpy(s->root, root, AFROS_SS_PATH_LEN - 1);
    s->stream_id = stream_id;
    s->mode      = mode;
    return (int32_t)(s - g_ss);
}

/**
 * @brief Stop sharing and free the session.
 */
int32_t StorageShareStop(int32_t sid)
{
    afros_ss_session_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    pthread_mutex_destroy(&s->lock);
    memset(s, 0, sizeof(*s));
    return AFROS_SUCCESS;
}

/**
 * @brief Read a file from the shared directory.
 * @param sid          Session id.
 * @param remote_path  Path relative to the share root.
 * @param out          Caller buffer.
 * @param cap          Capacity of @p out.
 * @param out_len      Receives bytes read.
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t StorageShareRead(int32_t sid, const char *remote_path,
                         uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    if (remote_path == NULL || out == NULL || out_len == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    afros_ss_session_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    char path[AFROS_SS_PATH_LEN];
    int32_t rc = resolve_path(s, remote_path, path, sizeof(path));
    if (rc != AFROS_SUCCESS) {
        return rc;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return AFROS_ERROR;
    }
    size_t n = fread(out, 1, cap, fp);
    fclose(fp);
    *out_len = (uint32_t)n;
    pthread_mutex_lock(&s->lock);
    s->bytes_read += (uint64_t)n;
    pthread_mutex_unlock(&s->lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Write a file into the shared directory (only if mode allows writes).
 */
int32_t StorageShareWrite(int32_t sid, const char *remote_path,
                          const uint8_t *data, uint32_t len)
{
    if (remote_path == NULL || (data == NULL && len > 0)) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    afros_ss_session_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    if (s->mode != AFROS_SS_MODE_READ_WRITE) {
        return AFROS_ERROR; /* Read-only share. */
    }
    char path[AFROS_SS_PATH_LEN];
    int32_t rc = resolve_path(s, remote_path, path, sizeof(path));
    if (rc != AFROS_SUCCESS) {
        return rc;
    }
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return AFROS_ERROR;
    }
    size_t n = (len > 0) ? fwrite(data, 1, len, fp) : 0;
    fclose(fp);
    pthread_mutex_lock(&s->lock);
    s->bytes_written += (uint64_t)n;
    pthread_mutex_unlock(&s->lock);
    return (n == len) ? AFROS_SUCCESS : AFROS_ERROR;
}

/** @brief Query the access mode of a share. */
int32_t StorageShareGetMode(int32_t sid, afros_ss_mode_t *out)
{
    if (out == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    afros_ss_session_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    *out = s->mode;
    return AFROS_SUCCESS;
}

/** @brief Snapshot cumulative byte counters for diagnostics. */
int32_t StorageShareGetStats(int32_t sid,
                             uint64_t *bytes_read,
                             uint64_t *bytes_written)
{
    afros_ss_session_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    pthread_mutex_lock(&s->lock);
    if (bytes_read != NULL)    *bytes_read    = s->bytes_read;
    if (bytes_written != NULL) *bytes_written = s->bytes_written;
    pthread_mutex_unlock(&s->lock);
    return AFROS_SUCCESS;
}
