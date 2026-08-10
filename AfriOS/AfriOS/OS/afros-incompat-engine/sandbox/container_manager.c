/**
 * @file container_manager.c
 * @brief Per-app container lifecycle: create/destroy/reset/enumerate.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int make_dirs(const char *path) {
    char buf[1024];
    strncpy(buf, path, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '/') buf[len - 1] = '\0';
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int rm_rf(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            char child[1024];
            snprintf(child, sizeof child, "%s/%s", path, e->d_name);
            struct stat st;
            if (stat(child, &st) == 0) {
                if (S_ISDIR(st.st_mode)) rm_rf(child);
                else unlink(child);
            }
        }
        closedir(d);
    }
    return rmdir(path);
}

/* ------------------------------------------------------------------ */
/* Standard container layout                                           */
/* ------------------------------------------------------------------ */

static const char *kContainerSubdirs[] = {
    "Data/Documents",
    "Data/Library",
    "Data/Library/Preferences",
    "Data/Library/Caches",
    "Data/Library/Application Support",
    "Data/tmp",
    "Data/Documents/Inbox",
    NULL
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t ContainerCreate(const char *bundle_id, char *path_out,
                               size_t len) {
    if (!bundle_id) return AFROS_ERROR_INVALID_PARAM;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[1024];
    snprintf(path, sizeof path, "%s/Library/Containers/%s", home, bundle_id);
    if (make_dirs(path) != 0) return AFROS_ERROR;
    for (int i = 0; kContainerSubdirs[i]; i++) {
        char sub[1280];
        snprintf(sub, sizeof sub, "%s/%s", path, kContainerSubdirs[i]);
        make_dirs(sub);
    }
    if (path_out && len) {
        strncpy(path_out, path, len - 1);
        path_out[len - 1] = '\0';
    }
    return AFROS_SUCCESS;
}

afros_status_t ContainerDestroy(const char *bundle_id) {
    if (!bundle_id) return AFROS_ERROR_INVALID_PARAM;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[1024];
    snprintf(path, sizeof path, "%s/Library/Containers/%s", home, bundle_id);
    if (rm_rf(path) != 0) return AFROS_ERROR;
    return AFROS_SUCCESS;
}

afros_status_t ContainerReset(const char *bundle_id) {
    afros_status_t s = ContainerDestroy(bundle_id);
    if (s != AFROS_SUCCESS) return s;
    return ContainerCreate(bundle_id, NULL, 0);
}

afros_status_t ContainerEnumerate(const char *bundle_id, char ***out,
                                  size_t *count) {
    if (!bundle_id || !out || !count) return AFROS_ERROR_INVALID_PARAM;
    *out   = NULL;
    *count = 0;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[1024];
    snprintf(path, sizeof path, "%s/Library/Containers/%s/Data", home,
             bundle_id);
    DIR *d = opendir(path);
    if (!d) return AFROS_ERROR_INVALID_PARAM;

    size_t cap = 8;
    char **arr = (char **)calloc(cap, sizeof(char *));
    if (!arr) { closedir(d); return AFROS_ERROR_NO_MEMORY; }
    size_t n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (n == cap) {
            cap *= 2;
            char **na = (char **)realloc(arr, cap * sizeof(char *));
            if (!na) {
                for (size_t i = 0; i < n; i++) free(arr[i]);
                free(arr);
                closedir(d);
                return AFROS_ERROR_NO_MEMORY;
            }
            arr = na;
        }
        arr[n++] = strdup(e->d_name);
    }
    closedir(d);
    *out   = arr;
    *count = n;
    return AFROS_SUCCESS;
}

afros_status_t ContainerCopyToDocuments(const char *bundle_id,
                                        const char *src,
                                        const char *name) {
    if (!bundle_id || !src || !name) return AFROS_ERROR_INVALID_PARAM;
    char dst[1024];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(dst, sizeof dst, "%s/Library/Containers/%s/Data/Documents/%s",
             home, bundle_id, name);
    FILE *in = fopen(src, "rb");
    if (!in) return AFROS_ERROR;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return AFROS_ERROR; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return AFROS_SUCCESS;
}

afros_status_t ContainerFreeList(char **list, size_t count) {
    if (!list) return AFROS_ERROR_INVALID_PARAM;
    for (size_t i = 0; i < count; i++) free(list[i]);
    free(list);
    return AFROS_SUCCESS;
}
