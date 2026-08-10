/**
 * @file bundle_manager.c
 * @brief Loader for Apple bundle layouts: .app, .framework, .kext.
 *
 * Parses the bundle's Info.plist, locates the main executable, and
 * registers the bundle so its identifier can be resolved later.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Bundle structure                                                    */
/* ------------------------------------------------------------------ */

typedef struct kv_pair_s {
    char *key;
    char *value;
    struct kv_pair_s *next;
} kv_pair_t;

struct apple_bundle_s {
    char       *path;            /* absolute bundle directory          */
    char       *identifier;      /* CFBundleIdentifier                 */
    char       *executable;      /* CFBundleExecutable                 */
    char       *version;         /* CFBundleVersion                    */
    char       *main_nib;        /* NSMainNibFile (optional)           */
    kv_pair_t  *info;            /* full key/value list                */
    bool        is_main;
};

#define AFROS_BUNDLE_MAX 32
static apple_bundle_t *g_bundles[AFROS_BUNDLE_MAX];
static uint32_t        g_bundle_count = 0;

/* ------------------------------------------------------------------ */
/* Minimal Info.plist XML parser                                       */
/* ------------------------------------------------------------------ */

static char *xml_extract_value(const char *xml, size_t len,
                               const char *key) {
    if (!xml || !key) return NULL;
    char pattern[256];
    snprintf(pattern, sizeof pattern, "<key>%s</key>", key);
    const char *p = strstr(xml, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    /* Skip whitespace.                                               */
    while (p < xml + len && (*p == ' ' || *p == '\n' || *p == '\t' ||
                             *p == '\r')) p++;
    /* Find next <string>...</string>.                                */
    const char *open = strstr(p, "<string>");
    if (!open) return NULL;
    open += strlen("<string>");
    const char *close = strstr(open, "</string>");
    if (!close) return NULL;
    size_t n = (size_t)(close - open);
    char *val = (char *)malloc(n + 1);
    if (!val) return NULL;
    memcpy(val, open, n);
    val[n] = '\0';
    return val;
}

static afros_status_t load_info_plist(apple_bundle_t *b) {
    char plist_path[1024];
    snprintf(plist_path, sizeof plist_path, "%s/Contents/Info.plist",
             b->path);
    int fd = open(plist_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        /* .framework layout may have Resources/Info.plist.           */
        snprintf(plist_path, sizeof plist_path,
                 "%s/Resources/Info.plist", b->path);
        fd = open(plist_path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) return AFROS_ERROR;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return AFROS_ERROR;
    }
    char *buf = (char *)malloc((size_t)st.st_size + 1);
    if (!buf) { close(fd); return AFROS_ERROR_NO_MEMORY; }
    ssize_t n = read(fd, buf, (size_t)st.st_size);
    close(fd);
    if (n <= 0) { free(buf); return AFROS_ERROR; }
    buf[n] = '\0';

    b->identifier = xml_extract_value(buf, (size_t)n, "CFBundleIdentifier");
    b->executable = xml_extract_value(buf, (size_t)n, "CFBundleExecutable");
    b->version    = xml_extract_value(buf, (size_t)n, "CFBundleVersion");
    b->main_nib   = xml_extract_value(buf, (size_t)n, "NSMainNibFile");
    free(buf);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t BundleLoad(const char *path, apple_bundle_t **out) {
    if (!path || !out) return AFROS_ERROR_INVALID_PARAM;
    *out = NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    apple_bundle_t *b = (apple_bundle_t *)calloc(1, sizeof *b);
    if (!b) return AFROS_ERROR_NO_MEMORY;
    b->path = strdup(path);
    if (load_info_plist(b) != AFROS_SUCCESS) {
        /* Not all bundles have an Info.plist — survive without it.    */
    }
    if (g_bundle_count < AFROS_BUNDLE_MAX) {
        g_bundles[g_bundle_count++] = b;
    }
    *out = b;
    return AFROS_SUCCESS;
}

apple_bundle_t *BundleGetMainBundle(void) {
    for (uint32_t i = 0; i < g_bundle_count; i++) {
        if (g_bundles[i] && g_bundles[i]->is_main) {
            return g_bundles[i];
        }
    }
    /* Fall back to the first registered bundle.                      */
    return g_bundle_count > 0 ? g_bundles[0] : NULL;
}

afros_status_t BundleSetMainBundle(apple_bundle_t *b) {
    if (!b) return AFROS_ERROR_INVALID_PARAM;
    for (uint32_t i = 0; i < g_bundle_count; i++) {
        if (g_bundles[i]) g_bundles[i]->is_main = false;
    }
    b->is_main = true;
    return AFROS_SUCCESS;
}

const char *BundleGetPath(apple_bundle_t *b) {
    return b ? b->path : NULL;
}

const char *BundleGetIdentifier(apple_bundle_t *b) {
    return b ? b->identifier : NULL;
}

const char *BundleGetExecutable(apple_bundle_t *b) {
    return b ? b->executable : NULL;
}

const char *BundleGetVersion(apple_bundle_t *b) {
    return b ? b->version : NULL;
}

const char *BundleGetMainNib(apple_bundle_t *b) {
    return b ? b->main_nib : NULL;
}

const char *BundleExecutablePath(apple_bundle_t *b, char *buf, size_t len) {
    if (!b || !buf || !len) return NULL;
    const char *exe = b->executable ? b->executable : "executable";
    snprintf(buf, len, "%s/Contents/MacOS/%s", b->path, exe);
    return buf;
}

const char *BundleResourcePath(apple_bundle_t *b, const char *name,
                               const char *type, char *buf, size_t len) {
    if (!b || !buf || !len) return NULL;
    if (type) {
        snprintf(buf, len, "%s/Contents/Resources/%s.%s", b->path, name, type);
    } else {
        snprintf(buf, len, "%s/Contents/Resources/%s", b->path, name);
    }
    return buf;
}

const char *BundleInfoGetString(apple_bundle_t *b, const char *key,
                                const char *fallback) {
    (void)b; (void)key;
    return fallback;
}

void BundleRelease(apple_bundle_t *b) {
    if (!b) return;
    for (uint32_t i = 0; i < g_bundle_count; i++) {
        if (g_bundles[i] == b) {
            g_bundles[i] = g_bundles[--g_bundle_count];
            break;
        }
    }
    kv_pair_t *kv = b->info;
    while (kv) {
        kv_pair_t *next = kv->next;
        free(kv->key);
        free(kv->value);
        free(kv);
        kv = next;
    }
    free(b->path);
    free(b->identifier);
    free(b->executable);
    free(b->version);
    free(b->main_nib);
    free(b);
}

afros_status_t BundleEnumerate(void (*cb)(apple_bundle_t *, void *),
                               void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    for (uint32_t i = 0; i < g_bundle_count; i++) {
        if (g_bundles[i]) cb(g_bundles[i], ctx);
    }
    return AFROS_SUCCESS;
}
