/*
 * registry.c — Côté serveur du registre Win32.
 *
 * Traite les requêtes registry reçues par le wineserver: open, query,
 * set, enumerate. Délègue au hive_manager pour l'accès aux données.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* --- Cache de valeurs récemment lues --------------------------------- */

typedef struct _REG_CACHE_ENTRY {
    HKEY     hive;
    char     path[256];
    char     name[128];
    ULONG    type;
    BYTE     data[1024];
    DWORD    data_len;
    ULONGLONG timestamp_ms;
    BOOL     valid;
} REG_CACHE_ENTRY;

#define REG_CACHE_SIZE 128
static REG_CACHE_ENTRY g_cache[REG_CACHE_SIZE];
static pthread_mutex_t g_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static ULONGLONG       g_now_ms = 0; /* simulé */

/* --- Helpers locaux ---------------------------------------------------- */

static ULONGLONG get_now_ms(void)
{
    return ++g_now_ms; /* incrémenté à chaque appel pour les tests */
}

/* Invalide les entrées de cache dont le TTL (5s) est expiré. */
static void prune_expired(void)
{
    int i;
    for (i = 0; i < REG_CACHE_SIZE; i++) {
        if (g_cache[i].valid && (g_now_ms - g_cache[i].timestamp_ms) > 5000)
            g_cache[i].valid = FALSE;
    }
}

/* --- API publique (côté serveur) ------------------------------------- */

/* Ouvre une clé pour un client. */
NTSTATUS ServerRegOpen(HKEY hive, const char *path, REG_KEY **out)
{
    LONG r = RegOpenKey(hive, path, out);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_OBJECT_NAME_NOT_FOUND;
}

/* Lit une valeur (avec cache 5s). */
NTSTATUS ServerRegQuery(REG_KEY *key, const char *name,
                        void *buf, DWORD *len)
{
    int i;
    ULONGLONG now;
    pthread_mutex_lock(&g_cache_lock);
    now = get_now_ms();
    prune_expired();
    for (i = 0; i < REG_CACHE_SIZE; i++) {
        if (g_cache[i].valid && g_cache[i].name[0] &&
            strcmp(g_cache[i].name, name ? name : "") == 0) {
            if (*len < g_cache[i].data_len) {
                pthread_mutex_unlock(&g_cache_lock);
                return STATUS_BUFFER_TOO_SMALL;
            }
            memcpy(buf, g_cache[i].data, g_cache[i].data_len);
            *len = g_cache[i].data_len;
            pthread_mutex_unlock(&g_cache_lock);
            return STATUS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_cache_lock);
    /* Cache miss: lecture réelle via RegQueryValue. */
    {
        LONG r = RegQueryValue(key, name, buf, len);
        if (r != ERROR_SUCCESS) return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    /* Mise en cache. */
    pthread_mutex_lock(&g_cache_lock);
    for (i = 0; i < REG_CACHE_SIZE; i++) {
        if (!g_cache[i].valid) {
            g_cache[i].valid = TRUE;
            g_cache[i].hive = key->hive;
            strncpy(g_cache[i].path, key->path, sizeof(g_cache[i].path) - 1);
            strncpy(g_cache[i].name, name ? name : "", sizeof(g_cache[i].name) - 1);
            if (*len <= sizeof(g_cache[i].data)) {
                memcpy(g_cache[i].data, buf, *len);
                g_cache[i].data_len = *len;
            }
            g_cache[i].timestamp_ms = now;
            break;
        }
    }
    pthread_mutex_unlock(&g_cache_lock);
    return STATUS_SUCCESS;
}

/* Écrit une valeur (invalide le cache). */
NTSTATUS ServerRegSet(REG_KEY *key, const char *name, ULONG type,
                      const void *data, DWORD len)
{
    int i;
    LONG r = RegSetValue(key, name, type, data, len);
    if (r != ERROR_SUCCESS) return STATUS_UNSUCCESSFUL;
    /* Invalide le cache pour ce nom. */
    pthread_mutex_lock(&g_cache_lock);
    for (i = 0; i < REG_CACHE_SIZE; i++) {
        if (g_cache[i].valid && g_cache[i].name[0] &&
            strcmp(g_cache[i].name, name ? name : "") == 0)
            g_cache[i].valid = FALSE;
    }
    pthread_mutex_unlock(&g_cache_lock);
    return STATUS_SUCCESS;
}

/* Ferme une clé. */
NTSTATUS ServerRegClose(REG_KEY *key)
{
    return (RegCloseKey(key) == ERROR_SUCCESS) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/* Énumère les sous-clés. */
NTSTATUS ServerRegEnumKey(REG_KEY *key, DWORD index, char *name, DWORD name_max)
{
    LONG r = RegEnumKey(key, index, name, name_max);
    return (r == ERROR_SUCCESS) ? STATUS_SUCCESS
         : (r == ERROR_NO_MORE_ITEMS) ? STATUS_NO_MORE_ENTRIES
         : STATUS_UNSUCCESSFUL;
}
