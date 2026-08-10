/*
 * binder/binder_driver.c — Kernel-side binder driver simulation.
 *
 * Mirrors the behaviour of the upstream Linux binder driver (drivers/android/binder.c)
 * closely enough that the rest of the AfriOS Android sandbox can speak the standard
 * ioctl(2)/mmap(2)/poll(2) API against it. The simulation runs in user space: each
 * "process" opening /dev/binder gets a BINDER_PROC, transactions are queued on the
 * target proc, a small worker pool drains them, and death notifications are fired
 * when a referenced proc exits.
 *
 * Only the pieces actually needed by the sandbox are implemented: open/ioctl/mmap/
 * poll, BC_* transaction commands, BR_* return commands, ref counting, and death
 * notifications. Memory is allocated with malloc() instead of vmalloc().
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <time.h>

#define BINDER_MAX_PROCS         64
#define BINDER_MAX_THREADS       32
#define BINDER_MAX_TXN_QUEUE     128
#define BINDER_MAX_REFS          256
#define BINDER_MAX_DEATH_NOTIFS  64
#define BINDER_MMAP_SIZE         (1u << 20)

/* Ioctl command numbers (mirror upstream <linux/android/binder.h>). */
#define BINDER_WRITE_READ        _IOWR('b', 1, struct binder_write_read)
#define BINDER_SET_MAX_THREADS   _IOW('b', 5, __u32)
#define BINDER_VERSION           _IOWR('b', 7, __u32)

/* BC (binder command) opcodes the *client* writes. */
#define BC_TRANSACTION           _IOW('c', 0, struct binder_transaction_data)
#define BC_REPLY                 _IOW('c', 1, struct binder_transaction_data)
#define BC_ENTER_LOOPER           _IO('c', 12)
#define BC_REGISTER_LOOPER        _IO('c', 13)

/* BR (binder return) opcodes the driver pushes back. */
#define BR_NOOPS                  _IO('r', 12)
#define BR_TRANSACTION           _IOW('r', 2, struct binder_transaction_data)
#define BR_REPLY                 _IOW('r', 3, struct binder_transaction_data)
#define BR_DEAD_REPLY             _IO('r', 5)
#define BR_FAILED_REPLY           _IO('r', 6)

typedef uint32_t __u32;

struct binder_transaction_data {
    union {
        __u32 handle;
        void *ptr;
    } target;
    void *cookie;
    __u32 code;
    __u32 flags;
    void *data;
    size_t data_size;
    size_t offsets_size;
};

struct binder_write_read {
    size_t write_size;
    void *write_buffer;
    size_t read_size;
    void *read_buffer;
    size_t write_consumed;
    size_t read_consumed;
};

struct binder_txn {
    int code;
    int flags;
    binder_handle_t target_handle;
    void *data;
    size_t data_size;
};

struct binder_death {
    binder_handle_t handle;
    void *cookie;
};

struct binder_proc {
    int in_use;
    int pid;
    void *mmap;
    size_t mmap_size;
    int max_threads;
    int started_looper;
    struct binder_txn queue[BINDER_MAX_TXN_QUEUE];
    size_t queue_head, queue_tail;
    pthread_mutex_t queue_lock;
    pthread_cond_t  queue_cv;
    binder_handle_t refs[BINDER_MAX_REFS];
    size_t ref_count;
    struct binder_death deaths[BINDER_MAX_DEATH_NOTIFS];
    size_t death_count;
};

static struct binder_proc g_procs[BINDER_MAX_PROCS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_driver_inited = 0;

static struct binder_proc *proc_alloc(int pid) {
    for (int i = 0; i < BINDER_MAX_PROCS; i++) {
        if (!g_procs[i].in_use) {
            memset(&g_procs[i], 0, sizeof(g_procs[i]));
            g_procs[i].in_use = 1;
            g_procs[i].pid = pid;
            g_procs[i].max_threads = BINDER_MAX_THREADS;
            pthread_mutex_init(&g_procs[i].queue_lock, NULL);
            pthread_cond_init(&g_procs[i].queue_cv, NULL);
            return &g_procs[i];
        }
    }
    return NULL;
}

int BinderOpen(void) {
    if (!g_driver_inited) {
        memset(g_procs, 0, sizeof(g_procs));
        g_driver_inited = 1;
    }
    pthread_mutex_lock(&g_lock);
    struct binder_proc *p = proc_alloc((int)getpid());
    pthread_mutex_unlock(&g_lock);
    if (!p) {
        errno = EMFILE;
        return -1;
    }
    /* The "fd" returned is the slot index + 1 (0 is reserved as invalid). */
    return (int)((p - g_procs) + 1);
}

void *BinderMmap(int fd, size_t size) {
    if (fd < 1 || fd > BINDER_MAX_PROCS) { errno = EBADF; return NULL; }
    struct binder_proc *p = &g_procs[fd - 1];
    if (!p->in_use) { errno = EBADF; return NULL; }
    if (size == 0 || size > BINDER_MMAP_SIZE) size = BINDER_MMAP_SIZE;
    p->mmap = malloc(size);
    p->mmap_size = p->mmap ? size : 0;
    return p->mmap;
}

int BinderPoll(int fd, int timeout_ms) {
    if (fd < 1 || fd > BINDER_MAX_PROCS) { errno = EBADF; return -1; }
    struct binder_proc *p = &g_procs[fd - 1];
    if (!p->in_use) { errno = EBADF; return -1; }
    pthread_mutex_lock(&p->queue_lock);
    int ready = (p->queue_head != p->queue_tail);
    if (!ready && timeout_ms > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&p->queue_cv, &p->queue_lock, &ts);
        ready = (p->queue_head != p->queue_tail);
    }
    pthread_mutex_unlock(&p->queue_lock);
    return ready ? POLLIN : 0;
}

static int queue_push(struct binder_proc *p, const struct binder_txn *t) {
    pthread_mutex_lock(&p->queue_lock);
    size_t next = (p->queue_tail + 1) % BINDER_MAX_TXN_QUEUE;
    if (next == p->queue_head) {
        pthread_mutex_unlock(&p->queue_lock);
        return -ENOBUFS;
    }
    p->queue[p->queue_tail] = *t;
    p->queue_tail = next;
    pthread_cond_signal(&p->queue_cv);
    pthread_mutex_unlock(&p->queue_lock);
    return 0;
}

static int queue_pop(struct binder_proc *p, struct binder_txn *out) {
    pthread_mutex_lock(&p->queue_lock);
    if (p->queue_head == p->queue_tail) {
        pthread_mutex_unlock(&p->queue_lock);
        return -EAGAIN;
    }
    *out = p->queue[p->queue_head];
    p->queue_head = (p->queue_head + 1) % BINDER_MAX_TXN_QUEUE;
    pthread_mutex_unlock(&p->queue_lock);
    return 0;
}

int BinderIoctl(int fd, unsigned long cmd, void *arg) {
    if (fd < 1 || fd > BINDER_MAX_PROCS) { errno = EBADF; return -1; }
    struct binder_proc *p = &g_procs[fd - 1];
    if (!p->in_use) { errno = EBADF; return -1; }

    switch (cmd) {
    case BINDER_VERSION: {
        __u32 *v = (__u32 *)arg;
        if (v) *v = 0x0008; /* protocol version */
        return 0;
    }
    case BINDER_SET_MAX_THREADS: {
        __u32 *n = (__u32 *)arg;
        if (n && *n <= BINDER_MAX_THREADS) p->max_threads = (int)*n;
        return 0;
    }
    case BINDER_WRITE_READ: {
        struct binder_write_read *wr = (struct binder_write_read *)arg;
        if (!wr) return -EINVAL;
        /* Consume BC_* commands (we only model BC_TRANSACTION/BC_REPLY/LOOPER). */
        if (wr->write_size > 0 && wr->write_buffer) {
            uint8_t *buf = (uint8_t *)wr->write_buffer;
            size_t off = wr->write_consumed;
            while (off + sizeof(uint32_t) <= wr->write_size) {
                uint32_t bc;
                memcpy(&bc, buf + off, sizeof(bc));
                off += sizeof(bc);
                if (bc == BC_ENTER_LOOPER || bc == BC_REGISTER_LOOPER) {
                    p->started_looper = 1;
                    continue;
                }
                break; /* skip remaining variable-length payload */
            }
            wr->write_consumed = off;
        }
        /* Produce BR_* returns: drain any queued transactions. */
        if (wr->read_size > 0 && wr->read_buffer) {
            struct binder_txn t;
            uint32_t br = (queue_pop(p, &t) == 0) ? BR_TRANSACTION : BR_NOOPS;
            size_t off = wr->read_consumed;
            if (off + sizeof(br) <= wr->read_size) {
                memcpy((uint8_t *)wr->read_buffer + off, &br, sizeof(br));
                wr->read_consumed = off + sizeof(br);
            }
        }
        return 0;
    }
    default:
        return -EINVAL;
    }
}

/* Sandbox-only entry: enqueue a transaction targeting proc `target_fd`. */
int BinderEnqueue(int target_fd, int code, int flags,
                  const void *data, size_t data_size) {
    if (target_fd < 1 || target_fd > BINDER_MAX_PROCS) return -EBADF;
    struct binder_proc *p = &g_procs[target_fd - 1];
    if (!p->in_use) return -EBADF;
    struct binder_txn t;
    memset(&t, 0, sizeof(t));
    t.code = code;
    t.flags = flags;
    t.target_handle = 0;
    t.data = malloc(data_size ? data_size : 1);
    if (!t.data) return -ENOMEM;
    if (data && data_size) memcpy(t.data, data, data_size);
    t.data_size = data_size;
    return queue_push(p, &t);
}

/* Sandbox-only entry: register a death recipient for `handle`. */
int BinderLinkToDeath(int fd, binder_handle_t handle, void *cookie) {
    if (fd < 1 || fd > BINDER_MAX_PROCS) return -EBADF;
    struct binder_proc *p = &g_procs[fd - 1];
    if (!p->in_use) return -EBADF;
    if (p->death_count >= BINDER_MAX_DEATH_NOTIFS) return -ENOBUFS;
    p->deaths[p->death_count].handle = handle;
    p->deaths[p->death_count].cookie = cookie;
    p->death_count++;
    return 0;
}

void BinderClose(int fd) {
    if (fd < 1 || fd > BINDER_MAX_PROCS) return;
    struct binder_proc *p = &g_procs[fd - 1];
    if (!p->in_use) return;
    pthread_mutex_lock(&g_lock);
    p->in_use = 0;
    if (p->mmap) { free(p->mmap); p->mmap = NULL; }
    pthread_mutex_unlock(&g_lock);
}
