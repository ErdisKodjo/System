#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>

#include "../include/loader.h"

/**
 * @file address_space.c
 * @brief Memory mapping registry across runtimes.
 *
 * Each runtime gets a private address space. We track mmap'd regions
 * per runtime in a registry, allow MAP_SHARED regions to be shared
 * between two runtimes (e.g. for IPC buffers, GPU textures, audio
 * rings), and provide unmap.
 *
 * On the host simulator, the registry uses real mmap() so cross-runtime
 * sharing actually works (two runtimes share the same host mapping).
 */

#define MAX_REGIONS 256

typedef struct {
    int             in_use;
    runtime_handle_t owner;
    void           *addr;
    size_t          len;
    int             prot;
    int             flags;
    int             fd;
    off_t           offset;
    int             shared;     /* 1 if MAP_SHARED */
    int             ref_count;  /* Number of runtimes sharing this */
} as_region_t;

static as_region_t g_regions[MAX_REGIONS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static as_region_t *region_find_locked(void *addr)
{
    for (int i = 0; i < MAX_REGIONS; i++)
        if (g_regions[i].in_use && g_regions[i].addr == addr)
            return &g_regions[i];
    return NULL;
}

static as_region_t *region_alloc_locked(void)
{
    for (int i = 0; i < MAX_REGIONS; i++)
        if (!g_regions[i].in_use) {
            g_regions[i].in_use = 1;
            return &g_regions[i];
        }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Reserve a region of memory in @p rt's address space.
 *
 * The region is backed by anonymous memory; nothing else can map into
 * the same address until AsUnmap or AsMap with MAP_FIXED replaces it.
 */
as_region_t *AsReserve(runtime_handle_t rt, void *hint, size_t len, int prot)
{
    as_region_t *r;
    void *addr;
    if (len == 0) { errno = EINVAL; return NULL; }

    pthread_mutex_lock(&g_lock);
    r = region_alloc_locked();
    if (!r) { pthread_mutex_unlock(&g_lock); return NULL; }

    addr = mmap(hint, len, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        r->in_use = 0;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }
    r->owner  = rt;
    r->addr   = addr;
    r->len    = len;
    r->prot   = prot;
    r->flags  = MAP_PRIVATE | MAP_ANONYMOUS;
    r->fd     = -1;
    r->offset = 0;
    r->shared = 0;
    r->ref_count = 1;
    pthread_mutex_unlock(&g_lock);
    return r;
}

/**
 * @brief Map a file or anonymous region into @p rt's address space.
 */
as_region_t *AsMap(runtime_handle_t rt, void *addr, size_t len,
                   int prot, int flags, int fd, off_t off)
{
    as_region_t *r;
    void *result;
    if (len == 0) { errno = EINVAL; return NULL; }

    pthread_mutex_lock(&g_lock);
    r = region_alloc_locked();
    if (!r) { pthread_mutex_unlock(&g_lock); return NULL; }

    result = mmap(addr, len, prot, flags, fd, off);
    if (result == MAP_FAILED) {
        r->in_use = 0;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }
    r->owner  = rt;
    r->addr   = result;
    r->len    = len;
    r->prot   = prot;
    r->flags  = flags;
    r->fd     = fd;
    r->offset = off;
    r->shared = (flags & MAP_SHARED) ? 1 : 0;
    r->ref_count = 1;
    pthread_mutex_unlock(&g_lock);
    return r;
}

/**
 * @brief Unmap a region. If the region is shared with other runtimes,
 *        its ref_count is decremented; only the last unmap actually
 *        releases the host mapping.
 */
afros_status_t AsUnmap(runtime_handle_t rt, void *addr, size_t len)
{
    as_region_t *r;
    afros_status_t rc = AFROS_SUCCESS;
    (void)len;
    pthread_mutex_lock(&g_lock);
    r = region_find_locked(addr);
    if (!r) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    if (r->owner != rt) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR; }
    r->ref_count--;
    if (r->ref_count <= 0) {
        if (munmap(r->addr, r->len) != 0)
            rc = AFROS_ERROR;
        memset(r, 0, sizeof(*r));
    }
    pthread_mutex_unlock(&g_lock);
    return rc;
}

/**
 * @brief Share a region owned by @p src with @p dst.
 *
 * If the region is MAP_SHARED, we mmap() the same backing file/memory
 * at the same offset into @p dst's "address space" — on the host
 * simulator both runtimes literally get the same host VM mapping, so
 * writes by one are visible to the other.
 *
 * For MAP_PRIVATE regions, we make a copy (copy-on-write not supported
 * cross-runtime).
 */
as_region_t *AsShare(runtime_handle_t src, runtime_handle_t dst,
                     void *addr, size_t len)
{
    as_region_t *r;
    as_region_t *r2;
    void *new_addr;
    pthread_mutex_lock(&g_lock);
    r = region_find_locked(addr);
    if (!r) { pthread_mutex_unlock(&g_lock); return NULL; }
    if (r->owner != src) { pthread_mutex_unlock(&g_lock); return NULL; }

    if (r->shared && r->fd >= 0) {
        /* Re-map the same backing file at the same offset. */
        new_addr = mmap(NULL, len, r->prot, r->flags, r->fd, r->offset);
        if (new_addr == MAP_FAILED) {
            pthread_mutex_unlock(&g_lock);
            return NULL;
        }
    } else {
        /* Anonymous: copy via mmap + memcpy. */
        new_addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (new_addr == MAP_FAILED) {
            pthread_mutex_unlock(&g_lock);
            return NULL;
        }
        memcpy(new_addr, r->addr, len);
    }

    r2 = region_alloc_locked();
    if (!r2) {
        munmap(new_addr, len);
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }
    r2->owner  = dst;
    r2->addr   = new_addr;
    r2->len    = len;
    r2->prot   = r->prot;
    r2->flags  = r->flags;
    r2->fd     = r->fd;
    r2->offset = r->offset;
    r2->shared = r->shared;
    r2->ref_count = 1;
    r->ref_count++;
    pthread_mutex_unlock(&g_lock);
    return r2;
}

/* ------------------------------------------------------------------ */
/* Introspection                                                      */
/* ------------------------------------------------------------------ */

uint32_t AsRegionCount(runtime_handle_t rt)
{
    uint32_t n = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_REGIONS; i++)
        if (g_regions[i].in_use && g_regions[i].owner == rt)
            n++;
    pthread_mutex_unlock(&g_lock);
    return n;
}

uint64_t AsRegionTotalBytes(runtime_handle_t rt)
{
    uint64_t total = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_REGIONS; i++)
        if (g_regions[i].in_use && g_regions[i].owner == rt)
            total += g_regions[i].len;
    pthread_mutex_unlock(&g_lock);
    return total;
}
