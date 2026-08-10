/*
 * vfs/ashmem.c — Android shared memory (/dev/ashmem).
 *
 * Android's ashmem ("anonymous shared memory") lets processes allocate
 * a region of memory that can be mapped into multiple address spaces by
 * passing the file descriptor over a binder transaction. Memory is
 * reference-counted by the kernel and can be paged out (pin/unpin) when
 * not actively in use.
 *
 * This module emulates ashmem in user space: each AshmemCreate() returns
 * a pseudo-fd backed by a tmpfs file under /tmp/afros-ashmem-*. mmap()
 * on the fd maps the underlying file; pin/unpin are no-ops (the sandbox
 * doesn't reclaim pages). The region's name and size are stored in a
 * global table so other processes can introspect them.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define ASHMEM_MAX_REGIONS 256
#define ASHMEM_NAME_MAX    128

struct ashmem_region {
    int   in_use;
    int   fd;
    void *addr;
    size_t size;
    char  name[ASHMEM_NAME_MAX];
    int   pinned;
};

static struct ashmem_region g_regions[ASHMEM_MAX_REGIONS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static int make_tmpfile(size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/afros-ashmem-%d", (int)getpid());
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -errno;
    unlink(path); /* unlink immediately so it goes away on close */
    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -errno;
    }
    return fd;
}

int AshmemCreate(const char *name, size_t size) {
    struct ashmem_region *slot = NULL;
    int fd;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (!g_regions[i].in_use) { slot = &g_regions[i]; break; }
    }
    if (!slot) { pthread_mutex_unlock(&g_lock); return -EMFILE; }
    if (size == 0) size = 4096;
    fd = make_tmpfile(size);
    if (fd < 0) { pthread_mutex_unlock(&g_lock); return fd; }
    slot->in_use = 1;
    slot->fd = fd;
    slot->addr = NULL;
    slot->size = size;
    slot->pinned = 1;
    snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "ashmem");
    pthread_mutex_unlock(&g_lock);
    return fd;
}

void *AshmemMmap(int fd, size_t size) {
    void *addr;
    struct ashmem_region *r = NULL;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return MAP_FAILED; }
    if (size == 0 || size > r->size) size = r->size;
    addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        pthread_mutex_unlock(&g_lock);
        return MAP_FAILED;
    }
    r->addr = addr;
    r->pinned = 1;
    pthread_mutex_unlock(&g_lock);
    return addr;
}

int AshmemPin(int fd, size_t offset, size_t len) {
    struct ashmem_region *r = NULL;
    (void)offset; (void)len;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return -ENOENT; }
    r->pinned = 1;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int AshmemUnpin(int fd, size_t offset, size_t len) {
    struct ashmem_region *r = NULL;
    (void)offset; (void)len;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return -ENOENT; }
    r->pinned = 0;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int AshmemGetSize(int fd) {
    struct ashmem_region *r = NULL;
    int sz;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return -ENOENT; }
    sz = (int)r->size;
    pthread_mutex_unlock(&g_lock);
    return sz;
}

int AshmemSetName(int fd, const char *name) {
    struct ashmem_region *r = NULL;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return -ENOENT; }
    snprintf(r->name, sizeof(r->name), "%s", name ? name : "ashmem");
    pthread_mutex_unlock(&g_lock);
    return 0;
}

const char *AshmemGetName(int fd) {
    static _Thread_local char buf[ASHMEM_NAME_MAX];
    struct ashmem_region *r = NULL;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return NULL; }
    snprintf(buf, sizeof(buf), "%s", r->name);
    pthread_mutex_unlock(&g_lock);
    return buf;
}

int AshmemClose(int fd) {
    struct ashmem_region *r = NULL;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) {
        if (g_regions[i].in_use && g_regions[i].fd == fd) { r = &g_regions[i]; break; }
    }
    if (!r) { pthread_mutex_unlock(&g_lock); return -ENOENT; }
    if (r->addr) { munmap(r->addr, r->size); r->addr = NULL; }
    close(r->fd);
    r->in_use = 0;
    r->fd = -1;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

size_t AshmemRegionCount(void) {
    size_t n = 0;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < ASHMEM_MAX_REGIONS; i++) if (g_regions[i].in_use) n++;
    pthread_mutex_unlock(&g_lock);
    return n;
}
