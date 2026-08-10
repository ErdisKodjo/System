/*
 * vfs/android_fs.c — Android filesystem emulation.
 *
 * Android apps expect a well-known filesystem layout:
 *
 *   /data/data/<pkg>/             app-private data (databases, prefs, files)
 *   /data/app/<pkg>-<n>/base.apk  installed APK
 *   /data/cache/                  system cache
 *   /sdcard/                      user external storage (FUSE)
 *   /system/                      read-only system image
 *   /proc/self/maps               Android-specific /proc layout
 *
 * This module sets up the directory tree under the sandbox's data root
 * (default /var/lib/afros-androsandbox) and provides path translation:
 * given a package name, return its data dir; given an Android-style path,
 * return the on-host path.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ANDROID_FS_MAX_PACKAGES 256
#define ANDROID_FS_PATH_MAX     512

static char g_root[ANDROID_FS_PATH_MAX] = "/var/lib/afros-androsandbox";
static int  g_inited = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* Ensure a directory exists at `path`, creating parents as needed. */
static int mkdir_p(const char *path, mode_t mode) {
    char tmp[ANDROID_FS_PATH_MAX];
    size_t len;
    if (!path || !*path) return -EINVAL;
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

int AndroidFsInit(const char *root) {
    static const char *kSubdirs[] = {
        "/data/data", "/data/app", "/data/cache", "/data/dalvik-cache",
        "/sdcard", "/system", "/system/app", "/system/fonts",
        "/system/framework", "/system/lib", "/system/lib64",
        "/proc/self", "/mnt/asec", "/mnt/obb", "/mnt/runtime",
        NULL,
    };
    char path[ANDROID_FS_PATH_MAX];
    FILE *f;

    pthread_mutex_lock(&g_lock);
    if (root && *root) {
        snprintf(g_root, sizeof(g_root), "%s", root);
    }
    for (size_t i = 0; kSubdirs[i]; i++) {
        snprintf(path, sizeof(path), "%s%s", g_root, kSubdirs[i]);
        mkdir_p(path, 0755);
    }
    /* Populate /proc/self/maps with a single synthetic mapping so the
     * Android linker's map parser is happy. */
    snprintf(path, sizeof(path), "%s/proc/self/maps", g_root);
    f = fopen(path, "w");
    if (f) {
        fprintf(f,
            "00010000-00110000 r-xp 00000000 00:00 0  [anon:dalvik]\n"
            "7f0000000000-7f0000100000 rw-p 00000000 00:00 0  [stack]\n");
        fclose(f);
    }
    g_inited = 1;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

/* Translate an Android-style path to the on-host path. */
int AndroidFsTranslate(const char *android_path, char *out, size_t out_max) {
    const char *p;
    if (!android_path || !out || out_max == 0) return -EINVAL;
    p = android_path;
    while (*p == '/') p++;
    if (strncmp(p, "data/", 5) == 0 ||
        strncmp(p, "system/", 7) == 0 ||
        strncmp(p, "sdcard/", 7) == 0 ||
        strncmp(p, "proc/", 5) == 0 ||
        strncmp(p, "mnt/", 4) == 0) {
        snprintf(out, out_max, "%s/%s", g_root, p);
    } else {
        snprintf(out, out_max, "%s/%s", g_root, android_path);
    }
    return 0;
}

/* Return (creating if needed) the data dir for `package_name`. */
int AndroidFsGetPathForPackage(const char *package_name,
                               char *out, size_t out_max) {
    static const char *kSub[] = {
        "/files", "/cache", "/code_cache", "/databases",
        "/shared_prefs", "/app_textures", "/app_dx9", "/app_obb", NULL,
    };
    char path[ANDROID_FS_PATH_MAX];
    char sub[ANDROID_FS_PATH_MAX];

    if (!package_name || !out || out_max == 0) return -EINVAL;
    if (!g_inited) AndroidFsInit(NULL);
    snprintf(path, sizeof(path), "%s/data/data/%s", g_root, package_name);
    mkdir_p(path, 0755);
    for (size_t i = 0; kSub[i]; i++) {
        snprintf(sub, sizeof(sub), "%s%s", path, kSub[i]);
        mkdir_p(sub, 0755);
    }
    snprintf(out, out_max, "%s", path);
    return 0;
}

/* Mark `path` as readable+writable by the package uid. Sandbox no-op. */
int AndroidFsSetPermissions(const char *path, const char *pkg) {
    (void)pkg;
    if (!path) return -EINVAL;
    return chmod(path, 0770);
}

/* Remove a package's data dir entirely (called on app uninstall). */
int AndroidFsWipePackage(const char *package_name) {
    char path[ANDROID_FS_PATH_MAX];
    char cmd[ANDROID_FS_PATH_MAX + 32];
    if (!package_name) return -EINVAL;
    snprintf(path, sizeof(path), "%s/data/data/%s", g_root, package_name);
    /* Recursively delete via system("rm -rf"). In the sandbox this is
     * safe because the path is under our root. */
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    return system(cmd);
}

const char *AndroidFsGetRoot(void) {
    return g_root;
}

/* Is `path` inside the sandbox root? Used by the syscall translator. */
int AndroidFsIsSandboxed(const char *path) {
    if (!path) return 0;
    return strncmp(path, g_root, strlen(g_root)) == 0 ? 1 : 0;
}

/* Install an APK copy into /data/app/<pkg>/base.apk. */
int AndroidFsInstallApk(const char *package_name, const char *src_apk) {
    char dst[ANDROID_FS_PATH_MAX];
    char dir[ANDROID_FS_PATH_MAX];
    char cmd[ANDROID_FS_PATH_MAX * 2 + 16];
    FILE *in, *out;
    char buf[8192];
    size_t n;

    if (!package_name || !src_apk) return -EINVAL;
    if (!g_inited) AndroidFsInit(NULL);
    snprintf(dir, sizeof(dir), "%s/data/app/%s", g_root, package_name);
    mkdir_p(dir, 0755);
    snprintf(dst, sizeof(dst), "%s/base.apk", dir);
    in = fopen(src_apk, "rb");
    if (!in) return -ENOENT;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return -EIO; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in); fclose(out); return -EIO;
        }
    }
    fclose(in); fclose(out);
    chmod(dst, 0644);
    (void)cmd;
    return 0;
}
