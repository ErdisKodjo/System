/*
 * surfaceflinger/buffer_queue.cpp — Producer/consumer graphic-buffer queue.
 *
 * Every Layer owns a BufferQueue. The producer side (the app, via
 * ANativeWindow::dequeueBuffer/queueBuffer) gets a writable slot, fills
 * it, and queues it; the consumer side (SurfaceFlinger, on vsync) takes
 * the most-recently queued buffer, composites it, and returns it to the
 * free pool.
 *
 * This implementation is a fixed-capacity ring of slots, each holding an
 * opaque byte blob (a stand-in for a real GraphicBuffer). The queue
 * drops the oldest buffer when full (so the producer never blocks) and
 * the consumer always reads the newest buffer (so the display latency
 * stays at one frame).
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <vector>
#include <mutex>

#define BQ_DEFAULT_CAPACITY 8

struct BufferSlot {
    uint8_t *data;
    size_t   len;
    int      frame_number;
};

struct BufferQueue {
    std::mutex mu;
    std::vector<BufferSlot> slots;
    size_t head;       /* consumer reads from here */
    size_t tail;       /* producer writes to here */
    size_t capacity;
    int    next_frame;
};

extern "C" void *BufferQueueCreate(int max) {
    BufferQueue *q = new BufferQueue();
    q->capacity = (max > 0 && max <= 64) ? (size_t)max : BQ_DEFAULT_CAPACITY;
    q->slots.resize(q->capacity);
    for (auto &s : q->slots) { s.data = nullptr; s.len = 0; s.frame_number = 0; }
    q->head = 0;
    q->tail = 0;
    q->next_frame = 1;
    return q;
}

extern "C" void BufferQueueDestroy(void *qp) {
    BufferQueue *q = static_cast<BufferQueue *>(qp);
    if (!q) return;
    for (auto &s : q->slots) if (s.data) std::free(s.data);
    delete q;
}

/* Producer: append a buffer; returns 0 on success. */
extern "C" int BufferQueueQueueBuffer(void *qp, const void *buf, size_t len) {
    BufferQueue *q = static_cast<BufferQueue *>(qp);
    if (!q) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(q->mu);
    /* If the queue is full, drop the oldest buffer. */
    size_t next = (q->tail + 1) % q->capacity;
    if (next == q->head) {
        std::free(q->slots[q->head].data);
        q->slots[q->head].data = nullptr;
        q->slots[q->head].len = 0;
        q->head = (q->head + 1) % q->capacity;
    }
    BufferSlot &s = q->slots[q->tail];
    s.data = (uint8_t *)std::malloc(len ? len : 1);
    if (!s.data) return NO_MEMORY;
    if (buf && len) std::memcpy(s.data, buf, len);
    s.len = len;
    s.frame_number = q->next_frame++;
    q->tail = next;
    return OK;
}

/* Consumer: read and remove the *newest* queued buffer; returns 0 if a
 * buffer was returned, WOULD_BLOCK if the queue is empty. */
extern "C" int BufferQueueDequeueBuffer(void *qp, void *out, size_t out_max,
                                        size_t *out_len) {
    BufferQueue *q = static_cast<BufferQueue *>(qp);
    if (!q) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(q->mu);
    if (q->head == q->tail) {
        if (out_len) *out_len = 0;
        return WOULD_BLOCK;
    }
    /* Find the newest slot by walking the ring. */
    size_t newest = q->head;
    int    best_frame = -1;
    for (size_t i = q->head; i != q->tail; i = (i + 1) % q->capacity) {
        if (q->slots[i].frame_number > best_frame) {
            best_frame = q->slots[i].frame_number;
            newest = i;
        }
    }
    BufferSlot &s = q->slots[newest];
    size_t cp = std::min(s.len, out_max);
    if (out && cp) std::memcpy(out, s.data, cp);
    if (out_len) *out_len = s.len;
    /* Free every slot up to and including `newest`. */
    while (q->head != (newest + 1) % q->capacity) {
        std::free(q->slots[q->head].data);
        q->slots[q->head].data = nullptr;
        q->slots[q->head].len = 0;
        q->head = (q->head + 1) % q->capacity;
    }
    return OK;
}

/* Free every queued buffer without reading them. */
extern "C" int BufferQueueFreeAll(void *qp) {
    BufferQueue *q = static_cast<BufferQueue *>(qp);
    if (!q) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(q->mu);
    while (q->head != q->tail) {
        std::free(q->slots[q->head].data);
        q->slots[q->head].data = nullptr;
        q->slots[q->head].len = 0;
        q->head = (q->head + 1) % q->capacity;
    }
    return OK;
}

extern "C" size_t BufferQueueDepth(void *qp) {
    BufferQueue *q = static_cast<BufferQueue *>(qp);
    if (!q) return 0;
    std::lock_guard<std::mutex> lk(q->mu);
    return (q->tail + q->capacity - q->head) % q->capacity;
}
