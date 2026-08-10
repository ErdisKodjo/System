/**
 * @file apfs_emulation.c
 * @brief Read-only APFS emulation.
 *
 * AfriOS exposes mounted iOS images as a read-only filesystem.
 * This module provides a minimal stat/read interface over the
 * underlying Linux file tree; APFS-specific features that cannot be
 * emulated return EOPNOTSUPP.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Mounted APFS volumes                                                */
/* ------------------------------------------------------------------ */

#define AFROS_APFS_MAX_VOLUMES 8

typedef struct {
    char device[256];
    char mountpoint[512];
    bool in_use;
} apfs_volume_t;

static apfs_volume_t g_apfs_volumes[AFROS_APFS_MAX_VOLUMES];

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const char *strip_mount(const char *path, const char *mountpoint) {
    size_t n = strlen(mountpoint);
    if (strncmp(path, mountpoint, n) == 0) {
        return path + n;
    }
    return path;
}

static const apfs_volume_t *find_volume_for(const char *path) {
    const apfs_volume_t *best = NULL;
    size_t best_len = 0;
    for (int i = 0; i < AFROS_APFS_MAX_VOLUMES; i++) {
        if (!g_apfs_volumes[i].in_use) continue;
        size_t n = strlen(g_apfs_volumes[i].mountpoint);
        if (strncmp(path, g_apfs_volumes[i].mountpoint, n) == 0 &&
            n > best_len) {
            best = &g_apfs_volumes[i];
            best_len = n;
        }
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t ApfsMount(const char *device, const char *mountpoint) {
    if (!device || !mountpoint) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_APFS_MAX_VOLUMES; i++) {
        if (!g_apfs_volumes[i].in_use) {
            strncpy(g_apfs_volumes[i].device, device,
                    sizeof g_apfs_volumes[i].device - 1);
            strncpy(g_apfs_volumes[i].mountpoint, mountpoint,
                    sizeof g_apfs_volumes[i].mountpoint - 1);
            g_apfs_volumes[i].in_use = true;
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR_NO_MEMORY;
}

afros_status_t ApfsUnmount(const char *mountpoint) {
    if (!mountpoint) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_APFS_MAX_VOLUMES; i++) {
        if (g_apfs_volumes[i].in_use &&
            strcmp(g_apfs_volumes[i].mountpoint, mountpoint) == 0) {
            g_apfs_volumes[i].in_use = false;
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR;
}

afros_status_t ApfsStat(const char *path, uint64_t *size_out) {
    if (!path || !size_out) return AFROS_ERROR_INVALID_PARAM;
    if (!find_volume_for(path)) return AFROS_ERROR_INVALID_PARAM;
    struct stat st;
    if (stat(path, &st) != 0) return AFROS_ERROR;
    *size_out = (uint64_t)st.st_size;
    return AFROS_SUCCESS;
}

afros_status_t ApfsRead(const char *path, uint64_t offset, void *buf,
                        size_t len) {
    if (!path || !buf) return AFROS_ERROR_INVALID_PARAM;
    if (!find_volume_for(path)) return AFROS_ERROR_INVALID_PARAM;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return AFROS_ERROR;
    if (lseek(fd, (off_t)offset, SEEK_SET) == (off_t)-1) {
        close(fd);
        return AFROS_ERROR;
    }
    ssize_t n = read(fd, buf, len);
    close(fd);
    if (n < 0 || (size_t)n != len) return AFROS_ERROR;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Unsupported APFS features                                           */
/* ------------------------------------------------------------------ */

afros_status_t ApfsCreateSnapshot(const char *mountpoint, const char *name) {
    (void)mountpoint; (void)name;
    /* AfriOS does not implement APFS snapshots. */
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t ApfsRestoreSnapshot(const char *mountpoint, const char *name) {
    (void)mountpoint; (void)name;
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t ApfsCloneFile(const char *src, const char *dst) {
    (void)src; (void)dst;
    /* Reflinks are not emulated. */
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t ApfsSparseAllocate(const char *path, uint64_t offset,
                                  uint64_t len) {
    (void)path; (void)offset; (void)len;
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t ApfsEnumerateVolumes(void (*cb)(const char *dev,
                                               const char *mount,
                                               void *ctx), void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_APFS_MAX_VOLUMES; i++) {
        if (g_apfs_volumes[i].in_use) {
            cb(g_apfs_volumes[i].device,
               g_apfs_volumes[i].mountpoint, ctx);
        }
    }
    return AFROS_SUCCESS;
}

const char *ApfsDescribeError(afros_status_t s) {
    switch (s) {
    case AFROS_ERROR_NOT_SUPPORTED: return "APFS feature not emulated";
    case AFROS_ERROR_INVALID_PARAM: return "invalid path";
    default: return "APFS error";
    }
    (void)strip_mount;
}
