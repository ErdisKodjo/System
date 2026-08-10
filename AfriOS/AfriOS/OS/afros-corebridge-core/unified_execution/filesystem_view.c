#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

#include "../include/runtime_manager.h"

/**
 * @file filesystem_view.c
 * @brief Unified VFS view across all runtimes.
 *
 * A runtime_mask_t selects which runtimes are visible in the view. The
 * view presents a single Unix-style namespace at "/afros/vfs" and exposes
 * path translation helpers (WinPathToUnix, UnixPathToWin, IOSPathToUnix)
 * plus open/read/write/close that transparently dispatch to the right
 * runtime's filesystem.
 *
 * Mount points:
 *   - Linux     /                  -> /afros/vfs/linux
 *   - Wine      Z: -> /  C: ->     -> /afros/vfs/wine
 *   - Android   /sdcard            -> /afros/vfs/android
 *   - iOS       sandbox container  -> /afros/vfs/ios
 *   - HarmonyOS /data              -> /afros/vfs/harmony
 */

#define VFS_ROOT "/afros/vfs"
#define MAX_VFS_OPEN 32

/* Bitmask of which runtimes are visible in a view. */
typedef uint32_t runtime_mask_t;
#define RMASK_NATIVE   (1u << RUNTIME_TYPE_NATIVE)
#define RMASK_LINUX    (1u << RUNTIME_TYPE_LINUX)
#define RMASK_WIN      (1u << RUNTIME_TYPE_WINBRIDGE)
#define RMASK_ANDROID  (1u << RUNTIME_TYPE_ANDROID)
#define RMASK_IOS      (1u << RUNTIME_TYPE_IOS)
#define RMASK_HARMONY  (1u << RUNTIME_TYPE_HARMONY)
#define RMASK_ALL      0xFFFFFFFFu

typedef struct {
    int  fd;
    char resolved[1024];
    int  writable;
} vfs_file_t;

typedef struct {
    runtime_mask_t mask;
    char           root[256];
    vfs_file_t     open_files[MAX_VFS_OPEN];
} vfs_view_t;

/* ------------------------------------------------------------------ */
/* Path translation                                                   */
/* ------------------------------------------------------------------ */

/** Translate a Windows path (C:\\foo\\bar or Z:\\foo) to a Unix path. */
const char *WinPathToUnix(const char *win)
{
    static char buf[1024];
    if (!win) return NULL;
    /* Drive letter: X:\... -> /afros/vfs/wine/<drive_lower>/... */
    if (win[0] && win[1] == ':' && (win[2] == '\\' || win[2] == '/')) {
        char drive = (char)tolower((unsigned char)win[0]);
        const char *rest = win + 3;
        char *p = buf;
        size_t i;
        snprintf(buf, sizeof(buf), "%s/wine/%c", VFS_ROOT, drive);
        i = strlen(buf);
        while (*rest && i + 1 < sizeof(buf)) {
            buf[i++] = (*rest == '\\') ? '/' : *rest;
            rest++;
        }
        buf[i] = '\0';
        (void)drive; (void)p;
        return buf;
    }
    /* UNC: \\server\share -> /afros/vfs/wine/unc/server/share */
    if (win[0] == '\\' && win[1] == '\\') {
        const char *rest = win + 2;
        size_t i = snprintf(buf, sizeof(buf), "%s/wine/unc", VFS_ROOT);
        while (*rest && i + 1 < sizeof(buf)) {
            buf[i++] = (*rest == '\\') ? '/' : *rest;
            rest++;
        }
        buf[i] = '\0';
        return buf;
    }
    snprintf(buf, sizeof(buf), "%s/wine/c/%s", VFS_ROOT, win);
    return buf;
}

/** Translate a Unix path to a Windows path. */
const char *UnixPathToWin(const char *unix_path)
{
    static char buf[1024];
    if (!unix_path) return NULL;
    if (strncmp(unix_path, VFS_ROOT "/wine/", strlen(VFS_ROOT) + 6) == 0) {
        const char *p = unix_path + strlen(VFS_ROOT) + 6;
        if (p[0] && p[1] == '/') {
            char drive = (char)toupper((unsigned char)p[0]);
            const char *rest = p + 1;
            size_t i = snprintf(buf, sizeof(buf), "%c:", drive);
            while (*rest && i + 1 < sizeof(buf)) {
                buf[i++] = (*rest == '/') ? '\\' : *rest;
                rest++;
            }
            buf[i] = '\0';
            return buf;
        }
    }
    /* Anything else maps to Z:\ (Wine's "Z:" drive = host root). */
    snprintf(buf, sizeof(buf), "Z:%s", unix_path);
    return buf;
}

/** Translate an iOS sandbox path (/var/mobile/Containers/...) to Unix. */
const char *IOSPathToUnix(const char *ios)
{
    static char buf[1024];
    if (!ios) return NULL;
    if (strncmp(ios, "/var/mobile/Containers/", 23) == 0) {
        snprintf(buf, sizeof(buf), "%s/ios/%s", VFS_ROOT, ios + 23);
    } else if (strncmp(ios, "/var/containers/", 16) == 0) {
        snprintf(buf, sizeof(buf), "%s/ios/%s", VFS_ROOT, ios + 16);
    } else {
        snprintf(buf, sizeof(buf), "%s/ios%s", VFS_ROOT, ios);
    }
    return buf;
}

/* ------------------------------------------------------------------ */
/* View management                                                    */
/* ------------------------------------------------------------------ */

vfs_view_t *VfsCreateView(runtime_mask_t mask)
{
    vfs_view_t *v = (vfs_view_t *)calloc(1, sizeof(*v));
    if (!v) return NULL;
    v->mask = mask;
    snprintf(v->root, sizeof(v->root), "%s", VFS_ROOT);
    /* Best-effort: create the mount root. */
    (void)mkdir(VFS_ROOT, 0755);
    if (mask & RMASK_LINUX)    { char p[256]; snprintf(p, sizeof(p), "%s/linux", VFS_ROOT); mkdir(p, 0755); }
    if (mask & RMASK_WIN)      { char p[256]; snprintf(p, sizeof(p), "%s/wine",  VFS_ROOT); mkdir(p, 0755); }
    if (mask & RMASK_ANDROID)  { char p[256]; snprintf(p, sizeof(p), "%s/android", VFS_ROOT); mkdir(p, 0755); }
    if (mask & RMASK_IOS)      { char p[256]; snprintf(p, sizeof(p), "%s/ios",   VFS_ROOT); mkdir(p, 0755); }
    if (mask & RMASK_HARMONY)  { char p[256]; snprintf(p, sizeof(p), "%s/harmony", VFS_ROOT); mkdir(p, 0755); }
    return v;
}

/* Resolve a virtual path inside the view to a host path. */
static const char *vfs_resolve(vfs_view_t *v, const char *path)
{
    static char buf[1024];
    if (!path) return NULL;
    if (path[0] == '/')
        snprintf(buf, sizeof(buf), "%s%s", v->root, path);
    else
        snprintf(buf, sizeof(buf), "%s/%s", v->root, path);
    return buf;
}

/* ------------------------------------------------------------------ */
/* File API                                                           */
/* ------------------------------------------------------------------ */

int VfsOpen(vfs_view_t *v, const char *path, int flags, int mode)
{
    const char *resolved;
    int fd;
    int slot = -1;

    if (!v || !path) return -1;
    for (int i = 0; i < MAX_VFS_OPEN; i++)
        if (!v->open_files[i].fd) { slot = i; break; }
    if (slot < 0) { errno = EMFILE; return -1; }

    resolved = vfs_resolve(v, path);
    fd = open(resolved, flags, mode);
    if (fd < 0) return -1;
    v->open_files[slot].fd = fd;
    strncpy(v->open_files[slot].resolved, resolved,
            sizeof(v->open_files[slot].resolved) - 1);
    v->open_files[slot].resolved[sizeof(v->open_files[slot].resolved) - 1] = '\0';
    v->open_files[slot].writable = (flags & (O_WRONLY | O_RDWR)) != 0;
    return slot + 1; /* opaque handle = slot+1 so 0 = invalid */
}

ssize_t VfsRead(vfs_view_t *v, int handle, void *buf, size_t len)
{
    if (!v || handle <= 0 || handle > MAX_VFS_OPEN) { errno = EBADF; return -1; }
    return read(v->open_files[handle - 1].fd, buf, len);
}

ssize_t VfsWrite(vfs_view_t *v, int handle, const void *buf, size_t len)
{
    if (!v || handle <= 0 || handle > MAX_VFS_OPEN) { errno = EBADF; return -1; }
    return write(v->open_files[handle - 1].fd, buf, len);
}

int VfsClose(vfs_view_t *v, int handle)
{
    int r;
    if (!v || handle <= 0 || handle > MAX_VFS_OPEN) { errno = EBADF; return -1; }
    r = close(v->open_files[handle - 1].fd);
    v->open_files[handle - 1].fd = 0;
    return r;
}

void VfsDestroyView(vfs_view_t *v)
{
    if (!v) return;
    for (int i = 0; i < MAX_VFS_OPEN; i++)
        if (v->open_files[i].fd)
            close(v->open_files[i].fd);
    free(v);
}
