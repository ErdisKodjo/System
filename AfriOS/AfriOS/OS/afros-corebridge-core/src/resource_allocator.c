#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "../include/orchestrator.h"
#include "../include/runtime_manager.h"
#include "../include/loader.h"
#include "../include/version_mgmt.h"

/**
 * @file resource_allocator.c
 * @brief Allocates system resources (memory pool, file descriptors,
 *        ports) per runtime based on quotas.
 *
 * A simple accounting layer: each runtime has a budget of FDs and
 * ports; allocation requests are granted up to the budget and refused
 * beyond it. Memory is allocated from a pre-reserved pool that we
 * carve up per-runtime.
 */

#define MAX_ALLOC_RT   32
#define MAX_TOTAL_FDS  4096
#define MAX_TOTAL_PORTS 65535
#define POOL_SIZE_BYTES (256ULL * 1024 * 1024) /* 256 MiB pool */

typedef enum {
    RES_TYPE_MEMORY = 1,
    RES_TYPE_FD,
    RES_TYPE_PORT,
} resource_type_t;

struct alloc_rt {
    int              in_use;
    runtime_handle_t rt;
    /* Budgets (taken from the quota_t if set, otherwise defaults). */
    uint32_t fd_budget;
    uint32_t port_budget;
    uint64_t mem_budget;
    /* Current allocations. */
    uint32_t fd_used;
    uint32_t port_used;
    uint64_t mem_used;
    /* Memory sub-pool: pointer + size carved from the global pool. */
    uint8_t *mem_base;
    uint64_t mem_size;
};

static struct alloc_rt g_rt[MAX_ALLOC_RT];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t        *g_pool = NULL;
static uint64_t        g_pool_used = 0;
static uint32_t        g_total_fds_used   = 0;
static uint32_t        g_total_ports_used = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static struct alloc_rt *find_locked(runtime_handle_t rt)
{
    for (int i = 0; i < MAX_ALLOC_RT; i++)
        if (g_rt[i].in_use && g_rt[i].rt == rt)
            return &g_rt[i];
    return NULL;
}

static struct alloc_rt *alloc_locked(runtime_handle_t rt)
{
    struct alloc_rt *s = find_locked(rt);
    if (s) return s;
    for (int i = 0; i < MAX_ALLOC_RT; i++)
        if (!g_rt[i].in_use) {
            memset(&g_rt[i], 0, sizeof(g_rt[i]));
            g_rt[i].in_use = 1;
            g_rt[i].rt     = rt;
            /* Defaults: 256 FDs, 64 ports, 32 MiB memory. */
            g_rt[i].fd_budget   = 256;
            g_rt[i].port_budget = 64;
            g_rt[i].mem_budget  = 32ULL * 1024 * 1024;
            return &g_rt[i];
        }
    return NULL;
}

static int ensure_pool(void)
{
    if (g_pool) return 0;
    g_pool = (uint8_t *)malloc(POOL_SIZE_BYTES);
    if (!g_pool) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Quota synchronization                                              */
/* ------------------------------------------------------------------ */

afros_status_t ResAllocApplyQuota(runtime_handle_t rt, const quota_t *q)
{
    struct alloc_rt *s;
    if (!q) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_lock);
    s = alloc_locked(rt);
    if (!s) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_NO_MEMORY; }
    if (q->fd_limit)   s->fd_budget   = q->fd_limit;
    if (q->port_limit) s->port_budget = q->port_limit;
    if (q->mem_limit_bytes) s->mem_budget = q->mem_limit_bytes;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Allocate @p amount of a resource type to a runtime.
 * @returns AFROS_SUCCESS on success, AFROS_ERROR if the budget is
 *          exhausted or the system total is exceeded.
 *
 * For RES_TYPE_MEMORY, @p amount is in bytes and the function returns
 * the offset of the allocated block within the runtime's sub-pool in
 * @p out_offset (if non-NULL).
 */
afros_status_t ResAlloc(runtime_handle_t rt, resource_type_t type,
                        size_t amount, size_t *out_offset)
{
    struct alloc_rt *s;
    afros_status_t rc = AFROS_SUCCESS;

    pthread_mutex_lock(&g_lock);
    s = alloc_locked(rt);
    if (!s) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_NO_MEMORY; }

    switch (type) {
    case RES_TYPE_MEMORY:
        if (ensure_pool() != 0) { rc = AFROS_ERROR_NO_MEMORY; break; }
        if (s->mem_used + amount > s->mem_budget) { rc = AFROS_ERROR; break; }
        if (g_pool_used + amount > POOL_SIZE_BYTES) { rc = AFROS_ERROR_NO_MEMORY; break; }
        {
            uint64_t off = g_pool_used;
            g_pool_used += amount;
            s->mem_used  += amount;
            if (out_offset) *out_offset = (size_t)off;
        }
        break;
    case RES_TYPE_FD:
        if (s->fd_used + amount > s->fd_budget) { rc = AFROS_ERROR; break; }
        if (g_total_fds_used + amount > MAX_TOTAL_FDS) { rc = AFROS_ERROR_NO_MEMORY; break; }
        s->fd_used       += (uint32_t)amount;
        g_total_fds_used += (uint32_t)amount;
        if (out_offset) *out_offset = s->fd_used;
        break;
    case RES_TYPE_PORT:
        if (s->port_used + amount > s->port_budget) { rc = AFROS_ERROR; break; }
        if (g_total_ports_used + amount > MAX_TOTAL_PORTS) { rc = AFROS_ERROR_NO_MEMORY; break; }
        s->port_used       += (uint32_t)amount;
        g_total_ports_used += (uint32_t)amount;
        if (out_offset) *out_offset = s->port_used;
        break;
    default:
        rc = AFROS_ERROR_INVALID_PARAM;
        break;
    }
    pthread_mutex_unlock(&g_lock);
    return rc;
}

afros_status_t ResFree(runtime_handle_t rt, resource_type_t type, size_t amount)
{
    struct alloc_rt *s;
    pthread_mutex_lock(&g_lock);
    s = find_locked(rt);
    if (!s) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    switch (type) {
    case RES_TYPE_MEMORY:
        if (s->mem_used < amount) amount = s->mem_used;
        s->mem_used -= amount;
        /* Note: pool offset compaction is not done here; the pool is
         * bump-allocated per-launch and reset only at teardown. */
        break;
    case RES_TYPE_FD:
        if (s->fd_used < amount) amount = s->fd_used;
        s->fd_used       -= (uint32_t)amount;
        g_total_fds_used -= (uint32_t)amount;
        break;
    case RES_TYPE_PORT:
        if (s->port_used < amount) amount = s->port_used;
        s->port_used       -= (uint32_t)amount;
        g_total_ports_used -= (uint32_t)amount;
        break;
    default:
        pthread_mutex_unlock(&g_lock);
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

uint64_t ResTotalAvailable(resource_type_t type)
{
    switch (type) {
    case RES_TYPE_MEMORY:
        return (g_pool ? POOL_SIZE_BYTES : POOL_SIZE_BYTES) - g_pool_used;
    case RES_TYPE_FD:
        return MAX_TOTAL_FDS - g_total_fds_used;
    case RES_TYPE_PORT:
        return MAX_TOTAL_PORTS - g_total_ports_used;
    default:
        return 0;
    }
}

afros_status_t ResAllocGetUsage(runtime_handle_t rt,
                                uint64_t *mem_used, uint32_t *fd_used,
                                uint32_t *port_used)
{
    struct alloc_rt *s;
    pthread_mutex_lock(&g_lock);
    s = find_locked(rt);
    if (!s) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    if (mem_used)   *mem_used   = s->mem_used;
    if (fd_used)    *fd_used    = s->fd_used;
    if (port_used)  *port_used  = s->port_used;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t ResAllocReset(runtime_handle_t rt)
{
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_ALLOC_RT; i++)
        if (g_rt[i].in_use && g_rt[i].rt == rt) {
            g_total_fds_used   -= g_rt[i].fd_used;
            g_total_ports_used -= g_rt[i].port_used;
            g_pool_used        -= g_rt[i].mem_used;
            g_rt[i].fd_used    = 0;
            g_rt[i].port_used  = 0;
            g_rt[i].mem_used   = 0;
            g_rt[i].in_use     = 0;
            break;
        }
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}
