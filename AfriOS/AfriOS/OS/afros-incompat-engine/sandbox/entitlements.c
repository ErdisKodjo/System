/**
 * @file entitlements.c
 * @brief Parse an Apple .entitlements plist (XML) and query keys.
 *
 * Entitlements are stored as a flat plist: an array of <key>/<value>
 * pairs at the top level. Only string and boolean values are supported
 * here, which covers every entitlement actually consumed by the
 * sandbox.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/* Storage                                                             */
/* ------------------------------------------------------------------ */

typedef struct ent_entry_s {
    char *key;
    char *value;     /* "true", "false", or a literal string          */
    bool  boolean;   /* true if the value is a boolean                 */
    struct ent_entry_s *next;
} ent_entry_t;

static ent_entry_t *g_ent_head = NULL;
static pthread_mutex_t g_ent_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

static char *extract_str(const char *start, const char *end) {
    const char *open = start;
    while (open < end && *open != '<') open++;
    if (open + 8 > end) return NULL;
    if (strncmp(open, "<string>", 8) != 0) return NULL;
    open += 8;
    const char *close = open;
    while (close + 9 <= end && strncmp(close, "</string>", 9) != 0) close++;
    if (close + 9 > end) return NULL;
    size_t n = (size_t)(close - open);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, open, n);
    out[n] = '\0';
    return out;
}

static bool extract_bool(const char *start, const char *end, bool *out) {
    const char *p = start;
    while (p < end && *p != '<') p++;
    if (p + 7 <= end && strncmp(p, "<true/", 6) == 0) { *out = true;  return true; }
    if (p + 8 <= end && strncmp(p, "<false/", 7) == 0) { *out = false; return true; }
    return false;
}

static void free_entries(void) {
    ent_entry_t *e = g_ent_head;
    while (e) {
        ent_entry_t *next = e->next;
        free(e->key);
        free(e->value);
        free(e);
        e = next;
    }
    g_ent_head = NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t EntitlementsLoad(const char *xml, size_t len) {
    if (!xml) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_ent_lock);
    free_entries();
    const char *p   = xml;
    const char *end = xml + len;

    while (p < end) {
        /* Find next <key> tag.                                       */
        const char *k = NULL;
        while (p < end) {
            k = strstr(p, "<key>");
            if (!k) { p = end; break; }
            p = k + 5;
            const char *kc = strstr(p, "</key>");
            if (!kc) { p = end; break; }
            size_t kn = (size_t)(kc - p);
            char *key = (char *)malloc(kn + 1);
            if (!key) { pthread_mutex_unlock(&g_ent_lock); return AFROS_ERROR_NO_MEMORY; }
            memcpy(key, p, kn);
            key[kn] = '\0';
            p = skip_ws(kc + 6, end);

            /* Parse the value.                                       */
            char *val = extract_str(p, end);
            bool bv = false;
            bool is_bool = (val == NULL) && extract_bool(p, end, &bv);

            ent_entry_t *e = (ent_entry_t *)calloc(1, sizeof *e);
            if (!e) { free(key); free(val); pthread_mutex_unlock(&g_ent_lock); return AFROS_ERROR_NO_MEMORY; }
            e->key = key;
            if (is_bool) {
                e->boolean = true;
                e->value   = strdup(bv ? "true" : "false");
            } else if (val) {
                e->boolean = false;
                e->value   = val;
            } else {
                e->boolean = false;
                e->value   = strdup("");
            }
            e->next = g_ent_head;
            g_ent_head = e;
            p = (val ? strstr(p, "</string>") : p) ;
            if (p) p = (p < end ? p + 9 : end);
            break;
        }
    }
    pthread_mutex_unlock(&g_ent_lock);
    return AFROS_SUCCESS;
}

afros_status_t EntitlementsLoadFromFile(const char *path) {
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    FILE *f = fopen(path, "rb");
    if (!f) return AFROS_ERROR;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return AFROS_ERROR; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return AFROS_ERROR_NO_MEMORY; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    afros_status_t s = EntitlementsLoad(buf, n);
    free(buf);
    return s;
}

bool EntitlementsHas(const char *key) {
    if (!key) return false;
    bool found = false;
    pthread_mutex_lock(&g_ent_lock);
    for (ent_entry_t *e = g_ent_head; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            /* For boolean entries, presence implies true unless the   */
            /* value is explicitly "false".                            */
            found = !(e->boolean && strcmp(e->value, "false") == 0);
            break;
        }
    }
    pthread_mutex_unlock(&g_ent_lock);
    return found;
}

const char *EntitlementsGetString(const char *key) {
    if (!key) return NULL;
    const char *out = NULL;
    pthread_mutex_lock(&g_ent_lock);
    for (ent_entry_t *e = g_ent_head; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            out = e->value;
            break;
        }
    }
    pthread_mutex_unlock(&g_ent_lock);
    return out;
}

afros_status_t EntitlementsEnumerate(void (*cb)(const char *key,
                                                const char *value,
                                                bool is_bool, void *ctx),
                                     void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_ent_lock);
    for (ent_entry_t *e = g_ent_head; e; e = e->next) {
        cb(e->key, e->value, e->boolean, ctx);
    }
    pthread_mutex_unlock(&g_ent_lock);
    return AFROS_SUCCESS;
}

void EntitlementsReset(void) {
    pthread_mutex_lock(&g_ent_lock);
    free_entries();
    pthread_mutex_unlock(&g_ent_lock);
}
