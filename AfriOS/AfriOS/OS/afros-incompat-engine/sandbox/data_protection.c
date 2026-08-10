/**
 * @file data_protection.c
 * @brief iOS file protection levels mapped to Unix permissions and an
 *        in-memory "encryption" flag.
 *
 * NSFileProtectionComplete UntilFirstUserAuthentication, etc. are
 * exposed as named constants; AfriOS records the level alongside
 * each path and reports it back to callers.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Public protection level names.                                     */
const char *const kDataProtectionComplete          = "NSFileProtectionComplete";
const char *const kDataProtectionCompleteUnlessOpen = "NSFileProtectionCompleteUnlessOpen";
const char *const kDataProtectionCompleteUntilFirstUserAuth =
    "NSFileProtectionCompleteUntilFirstUserAuthentication";
const char *const kDataProtectionNone              = "NSFileProtectionNone";

/* ------------------------------------------------------------------ */
/* Per-path level storage                                              */
/* ------------------------------------------------------------------ */

#define AFROS_DP_TABLE_BITS 8
#define AFROS_DP_TABLE_SIZE (1u << AFROS_DP_TABLE_BITS)
#define AFROS_DP_TABLE_MASK (AFROS_DP_TABLE_SIZE - 1)

typedef struct dp_entry_s {
    char *path;
    char *level;
    struct dp_entry_s *next;
} dp_entry_t;

static dp_entry_t *g_dp_table[AFROS_DP_TABLE_SIZE];
static pthread_mutex_t g_dp_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t dp_hash(const char *path) {
    uint32_t h = 2166136261u;
    for (; *path; path++) {
        h ^= (uint8_t)*path;
        h *= 16777619u;
    }
    return h & AFROS_DP_TABLE_MASK;
}

static const char *normalize_level(const char *level) {
    if (!level) return kDataProtectionCompleteUntilFirstUserAuth;
    if (strcmp(level, kDataProtectionComplete) == 0) return level;
    if (strcmp(level, kDataProtectionCompleteUnlessOpen) == 0) return level;
    if (strcmp(level, kDataProtectionCompleteUntilFirstUserAuth) == 0) return level;
    if (strcmp(level, kDataProtectionNone) == 0) return level;
    return kDataProtectionCompleteUntilFirstUserAuth;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t DataProtectSetLevel(const char *path, const char *level) {
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    const char *norm = normalize_level(level);
    uint32_t h = dp_hash(path);
    pthread_mutex_lock(&g_dp_lock);
    for (dp_entry_t *e = g_dp_table[h]; e; e = e->next) {
        if (strcmp(e->path, path) == 0) {
            char *new_level = strdup(norm);
            if (!new_level) {
                pthread_mutex_unlock(&g_dp_lock);
                return AFROS_ERROR_NO_MEMORY;
            }
            free(e->level);
            e->level = new_level;
            pthread_mutex_unlock(&g_dp_lock);
            return AFROS_SUCCESS;
        }
    }
    dp_entry_t *e = (dp_entry_t *)calloc(1, sizeof *e);
    if (!e) {
        pthread_mutex_unlock(&g_dp_lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    e->path  = strdup(path);
    e->level = strdup(norm);
    if (!e->path || !e->level) {
        free(e->path);
        free(e->level);
        free(e);
        pthread_mutex_unlock(&g_dp_lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    e->next = g_dp_table[h];
    g_dp_table[h] = e;
    pthread_mutex_unlock(&g_dp_lock);
    return AFROS_SUCCESS;
}

const char *DataProtectGetLevel(const char *path) {
    if (!path) return kDataProtectionCompleteUntilFirstUserAuth;
    uint32_t h = dp_hash(path);
    const char *out = kDataProtectionCompleteUntilFirstUserAuth;
    pthread_mutex_lock(&g_dp_lock);
    for (dp_entry_t *e = g_dp_table[h]; e; e = e->next) {
        if (strcmp(e->path, path) == 0) {
            out = e->level;
            break;
        }
    }
    pthread_mutex_unlock(&g_dp_lock);
    return out;
}

/* ------------------------------------------------------------------ */
/* Mapping to Unix permissions                                         */
/* ------------------------------------------------------------------ */

int DataProtectUnixMode(const char *level) {
    /* Complete protection requires the file to be unwritable while   */
    /* the device is locked; AfriOS approximates this by revoking     */
    /* write permission for group/other.                              */
    if (!level) return 0644;
    if (strcmp(level, kDataProtectionComplete) == 0) return 0600;
    if (strcmp(level, kDataProtectionCompleteUnlessOpen) == 0) return 0640;
    if (strcmp(level, kDataProtectionCompleteUntilFirstUserAuth) == 0) return 0644;
    if (strcmp(level, kDataProtectionNone) == 0) return 0666;
    return 0644;
}

bool DataProtectIsEncrypted(const char *path) {
    const char *level = DataProtectGetLevel(path);
    return strcmp(level, kDataProtectionNone) != 0;
}

afros_status_t DataProtectClearAll(void) {
    pthread_mutex_lock(&g_dp_lock);
    for (uint32_t i = 0; i < AFROS_DP_TABLE_SIZE; i++) {
        dp_entry_t *e = g_dp_table[i];
        while (e) {
            dp_entry_t *next = e->next;
            free(e->path);
            free(e->level);
            free(e);
            e = next;
        }
        g_dp_table[i] = NULL;
    }
    pthread_mutex_unlock(&g_dp_lock);
    return AFROS_SUCCESS;
}

afros_status_t DataProtectEnumerate(void (*cb)(const char *path,
                                               const char *level,
                                               void *ctx), void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_dp_lock);
    for (uint32_t i = 0; i < AFROS_DP_TABLE_SIZE; i++) {
        for (dp_entry_t *e = g_dp_table[i]; e; e = e->next) {
            cb(e->path, e->level, ctx);
        }
    }
    pthread_mutex_unlock(&g_dp_lock);
    return AFROS_SUCCESS;
}
