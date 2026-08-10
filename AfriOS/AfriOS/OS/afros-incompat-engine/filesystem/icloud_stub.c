/**
 * @file icloud_stub.c
 * @brief Stub for iCloud services.
 *
 * NSUbiquitousKeyValueStore and NSFileManager's
 * URLForUbiquityContainerIdentifier are not backed by a real cloud
 * service on AfriOS; they return empty/error to allow hosted apps
 * to start without crashing.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define AFROS_ICLOUD_KV_MAX 128

typedef struct kv_entry_s {
    char *key;
    char *value;
    struct kv_entry_s *next;
} kv_entry_t;

static kv_entry_t *g_kv_head = NULL;
static pthread_mutex_t g_kv_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_icloud_initialised = false;

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

afros_status_t IcloudInit(void) {
    if (g_icloud_initialised) return AFROS_SUCCESS;
    pthread_mutex_lock(&g_kv_lock);
    g_icloud_initialised = true;
    pthread_mutex_unlock(&g_kv_lock);
    return AFROS_SUCCESS;
}

afros_status_t IcloudShutdown(void) {
    pthread_mutex_lock(&g_kv_lock);
    kv_entry_t *kv = g_kv_head;
    while (kv) {
        kv_entry_t *next = kv->next;
        free(kv->key);
        free(kv->value);
        free(kv);
        kv = next;
    }
    g_kv_head = NULL;
    g_icloud_initialised = false;
    pthread_mutex_unlock(&g_kv_lock);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Key-value store                                                     */
/* ------------------------------------------------------------------ */

afros_status_t IcloudGetKeyValue(const char *key, char *buf, size_t *len) {
    if (!key || !len) return AFROS_ERROR_INVALID_PARAM;
    if (!g_icloud_initialised) IcloudInit();
    pthread_mutex_lock(&g_kv_lock);
    afros_status_t s = AFROS_ERROR;
    for (kv_entry_t *kv = g_kv_head; kv; kv = kv->next) {
        if (strcmp(kv->key, key) == 0) {
            size_t n = strlen(kv->value);
            if (buf && *len >= n + 1) {
                memcpy(buf, kv->value, n);
                buf[n] = '\0';
            }
            *len = n;
            s = AFROS_SUCCESS;
            break;
        }
    }
    if (s != AFROS_SUCCESS) *len = 0;
    pthread_mutex_unlock(&g_kv_lock);
    return s;
}

afros_status_t IcloudSetKeyValue(const char *key, const char *value) {
    if (!key || !value) return AFROS_ERROR_INVALID_PARAM;
    if (!g_icloud_initialised) IcloudInit();
    pthread_mutex_lock(&g_kv_lock);
    for (kv_entry_t *kv = g_kv_head; kv; kv = kv->next) {
        if (strcmp(kv->key, key) == 0) {
            free(kv->value);
            kv->value = strdup(value);
            pthread_mutex_unlock(&g_kv_lock);
            return AFROS_SUCCESS;
        }
    }
    kv_entry_t *kv = (kv_entry_t *)calloc(1, sizeof *kv);
    if (!kv) {
        pthread_mutex_unlock(&g_kv_lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    kv->key = strdup(key);
    kv->value = strdup(value);
    kv->next = g_kv_head;
    g_kv_head = kv;
    pthread_mutex_unlock(&g_kv_lock);
    return AFROS_SUCCESS;
}

afros_status_t IcloudDeleteKeyValue(const char *key) {
    if (!key) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_kv_lock);
    kv_entry_t **pp = &g_kv_head;
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            kv_entry_t *victim = *pp;
            *pp = victim->next;
            free(victim->key);
            free(victim->value);
            free(victim);
            pthread_mutex_unlock(&g_kv_lock);
            return AFROS_SUCCESS;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&g_kv_lock);
    return AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Ubiquity container                                                  */
/* ------------------------------------------------------------------ */

afros_status_t IcloudContainerUrl(const char *identifier, char *buf,
                                  size_t len) {
    if (!buf || !len) return AFROS_ERROR_INVALID_PARAM;
    /* Stub: return a placeholder container path so hosted apps       */
    /* receive a non-nil URL but never actually write to iCloud.       */
    snprintf(buf, len, "/var/mobile/Containers/iCloud/%s",
             identifier ? identifier : "default");
    return AFROS_SUCCESS;
}

afros_status_t IcloudEnumerateAll(void (*cb)(const char *key,
                                             const char *value,
                                             void *ctx), void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_kv_lock);
    for (kv_entry_t *kv = g_kv_head; kv; kv = kv->next) {
        cb(kv->key, kv->value, ctx);
    }
    pthread_mutex_unlock(&g_kv_lock);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Cloud documents sync (always reports success without doing work)   */
/* ------------------------------------------------------------------ */

afros_status_t IcloudUploadDocument(const char *path, const char *container_id) {
    (void)path; (void)container_id;
    /* Pretend to upload but actually no-op.                          */
    return AFROS_SUCCESS;
}

afros_status_t IcloudDownloadDocument(const char *container_id,
                                      const char *filename,
                                      char *buf, size_t len) {
    (void)container_id;
    if (!buf || !len) return AFROS_ERROR_INVALID_PARAM;
    snprintf(buf, len, "/var/mobile/Containers/iCloud/%s", filename);
    return AFROS_SUCCESS;
}
