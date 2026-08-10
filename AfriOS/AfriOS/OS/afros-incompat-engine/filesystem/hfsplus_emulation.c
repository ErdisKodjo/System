/**
 * @file hfsplus_emulation.c
 * @brief Read-only HFS+ emulation.
 *
 * Provides stat/read over legacy macOS disk images that use HFS+.
 * B-tree catalog and extents-overflow walks are stubbed: the engine
 * maps HFS+ paths directly to the underlying host filesystem.
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
/* HFS+ volume descriptor                                              */
/* ------------------------------------------------------------------ */

#define AFROS_HFS_MAX_VOLUMES 4

typedef struct hfs_volume_s {
    char     device[256];
    char     mountpoint[512];
    uint32_t block_size;
    uint64_t total_blocks;
    bool     in_use;
} hfs_volume_t;

static hfs_volume_t g_hfs_volumes[AFROS_HFS_MAX_VOLUMES];

/* ------------------------------------------------------------------ */
/* B-tree catalog stub                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t clut_head;
    uint32_t clut_leaf;
    uint32_t node_size;
} btree_info_t;

static btree_info_t g_catalog = { 0, 0, 4096 };
static btree_info_t g_extents = { 0, 0, 4096 };

afros_status_t hfs_init_btree(btree_info_t *bt, uint32_t node_size) {
    if (!bt) return AFROS_ERROR_INVALID_PARAM;
    bt->node_size = node_size;
    bt->clut_head = 0;
    bt->clut_leaf = 0;
    return AFROS_SUCCESS;
}

afros_status_t hfs_catalog_lookup(const char *path, uint64_t *cnid_out) {
    if (!path || !cnid_out) return AFROS_ERROR_INVALID_PARAM;
    /* Real HFS+ would walk the catalog B-tree; we synthesise a CNID   */
    /* from the path hash.                                             */
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const char *p = path; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 0x100000001b3ULL;
    }
    *cnid_out = (uint32_t)h;
    return AFROS_SUCCESS;
}

afros_status_t hfs_extents_lookup(uint64_t cnid, uint32_t fork,
                                  uint32_t *block_out, uint32_t *count_out) {
    (void)cnid; (void)fork;
    if (block_out) *block_out = 0;
    if (count_out) *count_out = 0;
    return AFROS_ERROR_NOT_SUPPORTED;
}

/* ------------------------------------------------------------------ */
/* Journal replay stub                                                 */
/* ------------------------------------------------------------------ */

static bool g_journal_initialised = false;

afros_status_t hfs_journal_init(hfs_volume_t *v) {
    (void)v;
    g_journal_initialised = true;
    return AFROS_SUCCESS;
}

afros_status_t hfs_journal_replay(hfs_volume_t *v) {
    (void)v;
    /* Read-only emulation: nothing to replay.                        */
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Volume helpers                                                      */
/* ------------------------------------------------------------------ */

static hfs_volume_t *find_volume(const char *path) {
    hfs_volume_t *best = NULL;
    size_t best_len = 0;
    for (int i = 0; i < AFROS_HFS_MAX_VOLUMES; i++) {
        if (!g_hfs_volumes[i].in_use) continue;
        size_t n = strlen(g_hfs_volumes[i].mountpoint);
        if (strncmp(path, g_hfs_volumes[i].mountpoint, n) == 0 &&
            n > best_len) {
            best = &g_hfs_volumes[i];
            best_len = n;
        }
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t HfsMount(const char *device, const char *mountpoint) {
    if (!device || !mountpoint) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_HFS_MAX_VOLUMES; i++) {
        if (!g_hfs_volumes[i].in_use) {
            strncpy(g_hfs_volumes[i].device, device,
                    sizeof g_hfs_volumes[i].device - 1);
            strncpy(g_hfs_volumes[i].mountpoint, mountpoint,
                    sizeof g_hfs_volumes[i].mountpoint - 1);
            g_hfs_volumes[i].block_size = 4096;
            g_hfs_volumes[i].in_use = true;
            hfs_init_btree(&g_catalog, 4096);
            hfs_init_btree(&g_extents, 4096);
            hfs_journal_init(&g_hfs_volumes[i]);
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR_NO_MEMORY;
}

afros_status_t HfsUnmount(const char *mountpoint) {
    if (!mountpoint) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_HFS_MAX_VOLUMES; i++) {
        if (g_hfs_volumes[i].in_use &&
            strcmp(g_hfs_volumes[i].mountpoint, mountpoint) == 0) {
            g_hfs_volumes[i].in_use = false;
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR;
}

afros_status_t HfsStat(const char *path, uint64_t *size_out) {
    if (!path || !size_out) return AFROS_ERROR_INVALID_PARAM;
    if (!find_volume(path)) return AFROS_ERROR_INVALID_PARAM;
    struct stat st;
    if (stat(path, &st) != 0) return AFROS_ERROR;
    *size_out = (uint64_t)st.st_size;
    return AFROS_SUCCESS;
}

afros_status_t HfsRead(const char *path, uint64_t offset, void *buf,
                       size_t len) {
    if (!path || !buf) return AFROS_ERROR_INVALID_PARAM;
    if (!find_volume(path)) return AFROS_ERROR_INVALID_PARAM;
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

afros_status_t HfsGetVolumeInfo(const char *mountpoint,
                                uint32_t *block_size,
                                uint64_t *total_blocks) {
    hfs_volume_t *v = NULL;
    for (int i = 0; i < AFROS_HFS_MAX_VOLUMES; i++) {
        if (g_hfs_volumes[i].in_use &&
            strcmp(g_hfs_volumes[i].mountpoint, mountpoint) == 0) {
            v = &g_hfs_volumes[i];
            break;
        }
    }
    if (!v) return AFROS_ERROR_INVALID_PARAM;
    if (block_size) *block_size = v->block_size;
    if (total_blocks) *total_blocks = v->total_blocks;
    return AFROS_SUCCESS;
}
