/*
 * registry_cache.c — Cache des valeurs du registre récemment lues (TTL 5s).
 *
 * Accélère les accès en lecture au registre en évitant de re-parcourir
 * les hives à chaque RegQueryValue. Les entrées expirent après 5 secondes.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define REG_CACHE_TTL_SEC    5
#define REG_CACHE_MAX_ENTRIES 256

/* --- Entrée de cache -------------------------------------------------- */

typedef struct _REG_CACHE_REC {
    BOOL          valid;
    HKEY          hive;
    char          path[200];
    char          name[128];
    ULONG         type;
    BYTE          data[512];
    DWORD         data_len;
    ULONGLONG     timestamp_sec;
} REG_CACHE_REC;

static REG_CACHE_REC   g_reg_cache[REG_CACHE_MAX_ENTRIES];
static pthread_mutex_t g_reg_cache_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- Helpers locaux ---------------------------------------------------- */

/* Heure courante en secondes (time(NULL) wrapped). */
static ULONGLONG now_seconds(void)
{
    return (ULONGLONG)time(NULL);
}

/* Invalide les entrées expirées. */
static void prune_expired(ULONGLONG now)
{
    int i;
    for (i = 0; i < REG_CACHE_MAX_ENTRIES; i++) {
        if (g_reg_cache[i].valid &&
            (now - g_reg_cache[i].timestamp_sec) > REG_CACHE_TTL_SEC)
            g_reg_cache[i].valid = FALSE;
    }
}

/* Trouve une entrée par (hive, path, name). */
static REG_CACHE_REC *find_entry(HKEY hive, const char *path, const char *name)
{
    int i;
    for (i = 0; i < REG_CACHE_MAX_ENTRIES; i++) {
        if (g_reg_cache[i].valid &&
            g_reg_cache[i].hive == hive &&
            strcmp(g_reg_cache[i].path, path ? path : "") == 0 &&
            strcmp(g_reg_cache[i].name, name ? name : "") == 0)
            return &g_reg_cache[i];
    }
    return NULL;
}

/* Alloue un slot libre (ou LRU). */
static REG_CACHE_REC *alloc_entry(void)
{
    int i;
    for (i = 0; i < REG_CACHE_MAX_ENTRIES; i++)
        if (!g_reg_cache[i].valid) return &g_reg_cache[i];
    /* Si tout est pris, on recycle l'entrée la plus ancienne. */
    {
        int victim = 0;
        ULONGLONG oldest = g_reg_cache[0].timestamp_sec;
        for (i = 1; i < REG_CACHE_MAX_ENTRIES; i++) {
            if (g_reg_cache[i].timestamp_sec < oldest) {
                oldest = g_reg_cache[i].timestamp_sec;
                victim = i;
            }
        }
        g_reg_cache[victim].valid = FALSE;
        return &g_reg_cache[victim];
    }
}

/* --- API publique ------------------------------------------------------ */

/* Recherche une valeur dans le cache. Retourne STATUS_SUCCESS si trouvée. */
NTSTATUS RegCacheGet(HKEY hive, const char *path, const char *name,
                     void *buf, DWORD *len, ULONG *type)
{
    REG_CACHE_REC *e;
    ULONGLONG now;
    NTSTATUS r = STATUS_NOT_FOUND;
    pthread_mutex_lock(&g_reg_cache_lock);
    now = now_seconds();
    prune_expired(now);
    e = find_entry(hive, path, name);
    if (e) {
        if (*len < e->data_len) {
            *len = e->data_len;
            r = STATUS_BUFFER_TOO_SMALL;
        } else {
            memcpy(buf, e->data, e->data_len);
            *len  = e->data_len;
            if (type) *type = e->type;
            r = STATUS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_reg_cache_lock);
    return r;
}

/* Insère une valeur dans le cache. */
NTSTATUS RegCachePut(HKEY hive, const char *path, const char *name,
                     ULONG type, const void *data, DWORD len)
{
    REG_CACHE_REC *e;
    ULONGLONG now;
    if (!path || !name || !data) return STATUS_INVALID_PARAMETER;
    if (len > sizeof(e->data)) return STATUS_BUFFER_TOO_SMALL;
    pthread_mutex_lock(&g_reg_cache_lock);
    now = now_seconds();
    prune_expired(now);
    e = find_entry(hive, path, name);
    if (!e) e = alloc_entry();
    if (!e) { pthread_mutex_unlock(&g_reg_cache_lock); return STATUS_INSUFFICIENT_RESOURCES; }
    e->valid         = TRUE;
    e->hive          = hive;
    e->type          = type;
    e->data_len      = len;
    e->timestamp_sec = now;
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    memcpy(e->data, data, len);
    pthread_mutex_unlock(&g_reg_cache_lock);
    return STATUS_SUCCESS;
}

/* Invalide une entrée spécifique (après écriture). */
NTSTATUS RegCacheInvalidate(HKEY hive, const char *path, const char *name)
{
    REG_CACHE_REC *e;
    pthread_mutex_lock(&g_reg_cache_lock);
    e = find_entry(hive, path, name);
    if (e) e->valid = FALSE;
    pthread_mutex_unlock(&g_reg_cache_lock);
    return STATUS_SUCCESS;
}

/* Vide tout le cache. */
NTSTATUS RegCacheFlush(void)
{
    pthread_mutex_lock(&g_reg_cache_lock);
    memset(g_reg_cache, 0, sizeof(g_reg_cache));
    pthread_mutex_unlock(&g_reg_cache_lock);
    return STATUS_SUCCESS;
}
