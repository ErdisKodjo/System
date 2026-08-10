/*
 * vfs/sdcard_emulation.c — /sdcard/ FUSE-like emulation.
 *
 * Android exposes external storage at /sdcard/, /storage/emulated/0/,
 * and /storage/self/primary/. On a real device these paths are backed by
 * a FUSE filesystem that translates accesses to the underlying ext4/f2fs
 * partition, applies the per-app storage restrictions, and forwards to
 * sdcardfs or fuse-daemon.
 *
 * In the sandbox we redirect all three paths to a single on-host
 * directory: $HOME/.afros/Android/sdcard (or /var/lib/afros-androsandbox/
 * sdcard if HOME is unset). SdcardInit() creates the tree; SdcardMount()
 * is a no-op that simply records the mount and returns the resolved
 * target path.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#define SDCARD_PATH_MAX 512

static char g_target[SDCARD_PATH_MAX] = "";
static int  g_mounted = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* Resolve the on-host directory that backs /sdcard/. */
static int resolve_target(char *out, size_t out_max) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    snprintf(out, out_max, "%s/.afros/Android/sdcard", home);
    return 0;
}

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[SDCARD_PATH_MAX];
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -errno;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -errno;
    return 0;
}

int SdcardInit(void) {
    char target[SDCARD_PATH_MAX];
    static const char *kSubdirs[] = {
        "/DCIM", "/DCIM/Camera", "/Pictures", "/Movies",
        "/Music", "/Download", "/Documents", "/Notifications",
        "/Alarms", "/Ringtones", "/Podcasts", "/Android",
        "/Android/data", "/Android/obb", "/Android/media",
        NULL,
    };
    char path[SDCARD_PATH_MAX];

    pthread_mutex_lock(&g_lock);
    resolve_target(target, sizeof(target));
    mkdir_p(target, 0755);
    for (size_t i = 0; kSubdirs[i]; i++) {
        snprintf(path, sizeof(path), "%s%s", target, kSubdirs[i]);
        mkdir_p(path, 0755);
    }
    snprintf(g_target, sizeof(g_target), "%s", target);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

/* "Mount" the sdcard: in the sandbox this just records that we're mounted
 * and returns the backing path in `out_target` (if non-NULL). */
int SdcardMount(const char * /*source*/, const char * /*target*/,
                char *out_target, size_t out_max) {
    pthread_mutex_lock(&g_lock);
    if (!g_target[0]) {
        resolve_target(g_target, sizeof(g_target));
        mkdir_p(g_target, 0755);
    }
    g_mounted = 1;
    if (out_target && out_max) {
        snprintf(out_target, out_max, "%s", g_target);
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int SdcardUnmount(void) {
    pthread_mutex_lock(&g_lock);
    g_mounted = 0;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int SdcardIsMounted(void) {
    pthread_mutex_lock(&g_lock);
    int m = g_mounted;
    pthread_mutex_unlock(&g_lock);
    return m;
}

const char *SdcardGetTarget(void) {
    if (!g_target[0]) SdcardInit();
    return g_target;
}

/* Translate an Android external-storage path to the on-host path. */
int SdcardTranslate(const char *android_path, char *out, size_t out_max) {
    const char *p;
    if (!android_path || !out || out_max == 0) return -EINVAL;
    if (!g_target[0]) SdcardInit();
    /* Strip any of the known Android external-storage prefixes. */
    static const char *kPrefixes[] = {
        "/sdcard/", "/storage/emulated/0/", "/storage/self/primary/",
        "/mnt/sdcard/", "/storage/sdcard0/", NULL,
    };
    p = NULL;
    for (size_t i = 0; kPrefixes[i]; i++) {
        size_t n = strlen(kPrefixes[i]);
        if (strncmp(android_path, kPrefixes[i], n) == 0) {
            p = android_path + n - 1; /* keep the leading '/' */
            break;
        }
    }
    if (!p) {
        /* No recognised prefix — assume it's already a relative path. */
        p = android_path;
        while (*p == '/') p++;
        if (*p == 0) p = "/";
        else p--;
    }
    snprintf(out, out_max, "%s%s", g_target, p);
    return 0;
}

/* Return the per-package external-storage dir (/sdcard/Android/data/<pkg>). */
int SdcardGetPathForPackage(const char *pkg, char *out, size_t out_max) {
    char path[SDCARD_PATH_MAX];
    if (!pkg || !out || out_max == 0) return -EINVAL;
    if (!g_target[0]) SdcardInit();
    snprintf(path, sizeof(path), "%s/Android/data/%s", g_target, pkg);
    mkdir_p(path, 0755);
    snprintf(out, out_max, "%s", path);
    return 0;
}
