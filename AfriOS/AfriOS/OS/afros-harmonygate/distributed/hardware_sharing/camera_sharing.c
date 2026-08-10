/**
 * @file camera_sharing.c
 * @brief AfriOS HarmonyOS compatibility — distributed camera sharing.
 *
 * Allows a remote device to subscribe to the local camera's frame stream.
 * The local side captures frames (V4L2 on hardware; synthetic YUV in the
 * sandbox), encodes them as length-prefixed datagrams, and ships them over
 * a SoftBus stream. The remote side pulls frames via CameraShareRecvFrame.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define AFROS_CAM_MAX_SESSIONS 4
#define AFROS_CAM_WIDTH        640
#define AFROS_CAM_HEIGHT       480
#define AFROS_CAM_FPS          15
#define AFROS_CAM_FRAME_BYTES  (AFROS_CAM_WIDTH * AFROS_CAM_HEIGHT * 3 / 2)

/** SoftBus stream send callback. */
typedef int32_t (*afros_cam_send_fn)(int32_t sid, const uint8_t *buf, uint32_t len);

typedef struct {
    int32_t          stream_id;
    afros_cam_send_fn send_fn;
    uint32_t         frame_seq;
    bool             capturing;
    bool             in_use;
    pthread_t        thread;
    pthread_mutex_t  lock;
} afros_cam_session_t;

static afros_cam_session_t g_cam[AFROS_CAM_MAX_SESSIONS];

static afros_cam_session_t *find_free(void)
{
    for (uint32_t i = 0; i < AFROS_CAM_MAX_SESSIONS; ++i) {
        if (!g_cam[i].in_use) {
            memset(&g_cam[i], 0, sizeof(g_cam[i]));
            g_cam[i].in_use = true;
            pthread_mutex_init(&g_cam[i].lock, NULL);
            return &g_cam[i];
        }
    }
    return NULL;
}

static afros_cam_session_t *get(int32_t id)
{
    if (id < 0 || id >= AFROS_CAM_MAX_SESSIONS) {
        return NULL;
    }
    if (!g_cam[id].in_use) {
        return NULL;
    }
    return &g_cam[id];
}

/** Produce a synthetic YUV420 frame: a moving grey gradient. */
static void synthesise_frame(uint8_t *out, uint32_t seq)
{
    uint8_t *y = out;
    uint8_t *u = out + AFROS_CAM_WIDTH * AFROS_CAM_HEIGHT;
    uint8_t *v = u + (AFROS_CAM_WIDTH * AFROS_CAM_HEIGHT) / 4;
    uint8_t base = (uint8_t)((seq * 4) & 0xFF);
    for (uint32_t row = 0; row < AFROS_CAM_HEIGHT; ++row) {
        for (uint32_t col = 0; col < AFROS_CAM_WIDTH; ++col) {
            y[row * AFROS_CAM_WIDTH + col] =
                (uint8_t)((row + col + base) & 0xFF);
        }
    }
    memset(u, 128, (AFROS_CAM_WIDTH * AFROS_CAM_HEIGHT) / 4);
    memset(v, 128, (AFROS_CAM_WIDTH * AFROS_CAM_HEIGHT) / 4);
}

/** Capture thread: pushes one frame every 1/FPS seconds. */
static void *capture_thread(void *arg)
{
    afros_cam_session_t *s = (afros_cam_session_t *)arg;
    static uint8_t frame[AFROS_CAM_FRAME_BYTES + 4];
    while (s->capturing) {
        pthread_mutex_lock(&s->lock);
        uint32_t seq = s->frame_seq++;
        pthread_mutex_unlock(&s->lock);
        synthesise_frame(frame + 4, seq);
        /* Length-prefix the frame so the receiver can split datagrams. */
        frame[0] = (uint8_t)(AFROS_CAM_FRAME_BYTES & 0xFF);
        frame[1] = (uint8_t)((AFROS_CAM_FRAME_BYTES >> 8) & 0xFF);
        frame[2] = (uint8_t)((AFROS_CAM_FRAME_BYTES >> 16) & 0xFF);
        frame[3] = (uint8_t)((AFROS_CAM_FRAME_BYTES >> 24) & 0xFF);
        s->send_fn(s->stream_id, frame, sizeof(frame));
        struct timespec ts = { .tv_sec = 0,
                               .tv_nsec = 1000000000L / AFROS_CAM_FPS };
        nanosleep(&ts, NULL);
    }
    return NULL;
}

/**
 * @brief Start sharing the local camera over a SoftBus stream.
 * @param stream_id  Stream id from StreamOpen().
 * @param send_fn    SoftBus stream send callback.
 * @return Session id ≥ 0, or -AFROS_ERROR_* on failure.
 */
int32_t CameraShareStart(int32_t stream_id, afros_cam_send_fn send_fn)
{
    if (send_fn == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    afros_cam_session_t *s = find_free();
    if (s == NULL) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    s->stream_id = stream_id;
    s->send_fn   = send_fn;
    s->frame_seq = 0;
    s->capturing = true;
    if (pthread_create(&s->thread, NULL, capture_thread, s) != 0) {
        s->in_use = false;
        return -AFROS_ERROR;
    }
    return (int32_t)(s - g_cam);
}

/**
 * @brief Stop sharing and free the session.
 */
int32_t CameraShareStop(int32_t sid)
{
    afros_cam_session_t *s = get(sid);
    if (s == NULL) {
        return AFROS_ERROR;
    }
    s->capturing = false;
    pthread_join(s->thread, NULL);
    pthread_mutex_destroy(&s->lock);
    memset(s, 0, sizeof(*s));
    return AFROS_SUCCESS;
}

/**
 * @brief Pull the next frame from a SoftBus stream on the receiver side.
 * @param sid        Local capture session id (always 0 on receivers).
 * @param recv_fn    SoftBus stream recv callback.
 * @param out        Caller buffer (≥ AFROS_CAM_FRAME_BYTES).
 * @param cap        Capacity of @p out.
 * @param out_len    Receives the number of YUV bytes written.
 * @return AFROS_SUCCESS or an AFROS_ERROR_* code.
 */
int32_t CameraShareRecvFrame(int32_t sid,
                             int32_t (*recv_fn)(int32_t, uint8_t *, uint32_t),
                             uint8_t *out,
                             uint32_t cap,
                             uint32_t *out_len)
{
    (void)sid;
    if (recv_fn == NULL || out == NULL || out_len == NULL || cap == 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    static uint8_t buf[AFROS_CAM_FRAME_BYTES + 4];
    int32_t n = recv_fn(0, buf, sizeof(buf));
    if (n < 4) {
        return AFROS_ERROR_TIMEOUT;
    }
    uint32_t len = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                   ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    if (len > cap || len > (uint32_t)n - 4) {
        return AFROS_ERROR_NO_MEMORY;
    }
    memcpy(out, buf + 4, len);
    *out_len = len;
    return AFROS_SUCCESS;
}

/** @brief Query the dimensions advertised by the camera share. */
int32_t CameraShareGetFormat(uint32_t *width, uint32_t *height, uint8_t *fps)
{
    if (width != NULL)  *width  = AFROS_CAM_WIDTH;
    if (height != NULL) *height = AFROS_CAM_HEIGHT;
    if (fps != NULL)    *fps    = AFROS_CAM_FPS;
    return AFROS_SUCCESS;
}

/** @brief Number of frames captured so far on a session. */
uint32_t CameraShareFrameCount(int32_t sid)
{
    afros_cam_session_t *s = get(sid);
    if (s == NULL) {
        return 0;
    }
    pthread_mutex_lock(&s->lock);
    uint32_t n = s->frame_seq;
    pthread_mutex_unlock(&s->lock);
    return n;
}
