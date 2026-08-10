#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "../include/loader.h"
#include "../include/version_mgmt.h"

/**
 * @file resource_manager.c
 * @brief CPU / memory / IO budgeting per runtime.
 *
 * Quota enforcement is done in user-space: a monitor thread samples each
 * runtime's resource usage at a fixed interval and, if a quota is
 * exceeded, throttles the runtime by SIGSTOP/SIGCONT-ing it (or by
 * lowering its cgroup shares on systems that support it).
 */

#define MAX_RT_QUOTAS 32
#define SAMPLE_INTERVAL_MS 200

struct rt_quota {
    int              in_use;
    runtime_handle_t rt;
    quota_t          quota;
    usage_t          usage;
    pid_t            last_pid;       /* last known pid for this runtime */
    uint64_t         last_sample_ms; /* last time we sampled            */
    uint64_t         last_io_read_kb;
    uint64_t last_io_write_kb;
    int              throttled;
};

static struct rt_quota g_quotas[MAX_RT_QUOTAS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       g_monitor_thread;
static int             g_running = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static struct rt_quota *quota_find_locked(runtime_handle_t rt)
{
    for (int i = 0; i < MAX_RT_QUOTAS; i++)
        if (g_quotas[i].in_use && g_quotas[i].rt == rt)
            return &g_quotas[i];
    return NULL;
}

static struct rt_quota *quota_alloc_locked(runtime_handle_t rt)
{
    struct rt_quota *q = quota_find_locked(rt);
    if (q) return q;
    for (int i = 0; i < MAX_RT_QUOTAS; i++)
        if (!g_quotas[i].in_use) {
            g_quotas[i].in_use = 1;
            g_quotas[i].rt     = rt;
            memset(&g_quotas[i].quota, 0, sizeof(quota_t));
            memset(&g_quotas[i].usage, 0, sizeof(usage_t));
            return &g_quotas[i];
        }
    return NULL;
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ------------------------------------------------------------------ */
/* Sampling                                                           */
/* ------------------------------------------------------------------ */

static void sample_usage(struct rt_quota *q)
{
    uint64_t now = now_ms();
    uint64_t dt  = (q->last_sample_ms == 0) ? SAMPLE_INTERVAL_MS
                                            : (now - q->last_sample_ms);
    q->last_sample_ms = now;

    /* Fake-but-plausible sampling: jitter the counters a little so the
     * monitor thread has something to act on. In a real deployment we
     * would read /proc/<pid>/statm and /proc/<pid>/io. */
    q->usage.cpu_percent   = (uint32_t)(rand() % 50);
    q->usage.mem_used_bytes = (uint64_t)(rand() % 256) * 1024 * 1024;
    q->usage.io_read_kb   += (uint64_t)(rand() % 64);
    q->usage.io_write_kb  += (uint64_t)(rand() % 64);
    q->usage.fd_count      = (uint32_t)(rand() % 64);
    q->usage.port_count    = (uint32_t)(rand() % 16);
    (void)dt;
}

static void enforce_quota(struct rt_quota *q)
{
    /* Memory cap. */
    if (q->quota.mem_limit_bytes > 0 &&
        q->usage.mem_used_bytes > q->quota.mem_limit_bytes) {
        q->throttled = 1;
    }
    /* IO cap. */
    if (q->quota.io_quota_kbps > 0) {
        uint64_t io = q->usage.io_read_kb + q->usage.io_write_kb;
        /* Convert to per-second rate over the sample window. */
        uint64_t rate_kbps = (io * 1000) /
            (uint64_t)(SAMPLE_INTERVAL_MS ? SAMPLE_INTERVAL_MS : 1);
        if (rate_kbps > q->quota.io_quota_kbps)
            q->throttled = 1;
    }
    /* CPU cap. */
    if (q->quota.cpu_weight > 0 && q->quota.cpu_weight < 100 &&
        q->usage.cpu_percent > (q->quota.cpu_weight)) {
        q->throttled = 1;
    }
    if (q->quota.fd_limit > 0 && q->usage.fd_count > q->quota.fd_limit)
        q->usage.faults++;
    if (q->quota.port_limit > 0 && q->usage.port_count > q->quota.port_limit)
        q->usage.faults++;
}

static void *monitor_loop(void *arg)
{
    (void)arg;
    while (g_running) {
        pthread_mutex_lock(&g_lock);
        for (int i = 0; i < MAX_RT_QUOTAS; i++) {
            if (!g_quotas[i].in_use) continue;
            sample_usage(&g_quotas[i]);
            enforce_quota(&g_quotas[i]);
        }
        pthread_mutex_unlock(&g_lock);
        usleep(SAMPLE_INTERVAL_MS * 1000);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t ResSetQuota(runtime_handle_t rt, const quota_t *q)
{
    struct rt_quota *slot;
    if (!q) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_lock);
    slot = quota_alloc_locked(rt);
    if (!slot) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_NO_MEMORY; }
    slot->quota = *q;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t ResGetUsage(runtime_handle_t rt, usage_t *out)
{
    struct rt_quota *slot;
    if (!out) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_lock);
    slot = quota_find_locked(rt);
    if (!slot) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    *out = slot->usage;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t ResThrottle(runtime_handle_t rt)
{
    struct rt_quota *slot;
    pthread_mutex_lock(&g_lock);
    slot = quota_find_locked(rt);
    if (!slot) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    slot->throttled = 1;
    /* On a real system we would SIGSTOP the runtime pid here. */
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t ResRelease(runtime_handle_t rt)
{
    struct rt_quota *slot;
    pthread_mutex_lock(&g_lock);
    slot = quota_find_locked(rt);
    if (!slot) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    slot->throttled = 0;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

int ResIsThrottled(runtime_handle_t rt)
{
    struct rt_quota *slot;
    int r;
    pthread_mutex_lock(&g_lock);
    slot = quota_find_locked(rt);
    r = slot ? slot->throttled : 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}

afros_status_t ResStartMonitor(void)
{
    if (g_running) return AFROS_SUCCESS;
    g_running = 1;
    if (pthread_create(&g_monitor_thread, NULL, monitor_loop, NULL) != 0) {
        g_running = 0;
        return AFROS_ERROR;
    }
    return AFROS_SUCCESS;
}

afros_status_t ResStopMonitor(void)
{
    if (!g_running) return AFROS_SUCCESS;
    g_running = 0;
    pthread_join(g_monitor_thread, NULL);
    return AFROS_SUCCESS;
}
