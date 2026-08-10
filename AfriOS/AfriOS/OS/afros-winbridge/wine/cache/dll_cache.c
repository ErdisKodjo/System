/*
 * dll_cache.c — Cache en mémoire des DLLs chargées (avec refcount).
 *
 * Évite de recharger une DLL déjà en mémoire: chaque entrée du cache
 * associe un nom de DLL à un PE_MODULE et un compteur de références.
 * Quand le refcount tombe à zéro, la DLL peut être déchargée.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_DLL_CACHE 128

/* --- Entrée de cache -------------------------------------------------- */

typedef struct _DLL_CACHE_ENTRY {
    char       name[64];
    PE_MODULE *module;
    HANDLE     handle;     /* dlopen handle, si applicable */
    int        refcount;
    ULONGLONG  load_time;
} DLL_CACHE_ENTRY;

static DLL_CACHE_ENTRY  g_dll_cache[MAX_DLL_CACHE];
static int              g_dll_count = 0;
static pthread_mutex_t  g_dll_lock  = PTHREAD_MUTEX_INITIALIZER;

/* --- Helpers locaux ---------------------------------------------------- */

static DLL_CACHE_ENTRY *find_entry(const char *name)
{
    int i;
    for (i = 0; i < g_dll_count; i++)
        if (strcmp(g_dll_cache[i].name, name) == 0)
            return &g_dll_cache[i];
    return NULL;
}

static DLL_CACHE_ENTRY *alloc_entry(void)
{
    if (g_dll_count >= MAX_DLL_CACHE) return NULL;
    return &g_dll_cache[g_dll_count++];
}

/* --- API publique ------------------------------------------------------ */

/* Récupère une DLL du cache (incrémentant le refcount). Retourne NULL
 * si la DLL n'est pas en cache. */
PE_MODULE *DllCacheGet(const char *name)
{
    DLL_CACHE_ENTRY *e;
    PE_MODULE *m = NULL;
    pthread_mutex_lock(&g_dll_lock);
    e = find_entry(name);
    if (e) {
        e->refcount++;
        m = e->module;
    }
    pthread_mutex_unlock(&g_dll_lock);
    return m;
}

/* Insère une DLL dans le cache (refcount initial = 1). */
NTSTATUS DllCachePut(const char *name, PE_MODULE *mod, HANDLE handle)
{
    DLL_CACHE_ENTRY *e;
    NTSTATUS r = STATUS_SUCCESS;
    pthread_mutex_lock(&g_dll_lock);
    e = find_entry(name);
    if (e) {
        e->refcount++;
        r = STATUS_OBJECT_NAME_COLLISION;
    } else {
        e = alloc_entry();
        if (!e) r = STATUS_INSUFFICIENT_RESOURCES;
        else {
            memset(e, 0, sizeof(*e));
            strncpy(e->name, name, sizeof(e->name) - 1);
            e->module   = mod;
            e->handle   = handle;
            e->refcount = 1;
            e->load_time = 0;
        }
    }
    pthread_mutex_unlock(&g_dll_lock);
    return r;
}

/* Décrémente le refcount d'une DLL. Si 0, libère les ressources. */
NTSTATUS DllCacheRelease(const char *name)
{
    DLL_CACHE_ENTRY *e;
    NTSTATUS r = STATUS_NOT_FOUND;
    pthread_mutex_lock(&g_dll_lock);
    e = find_entry(name);
    if (e) {
        e->refcount--;
        if (e->refcount <= 0) {
            /* En pratique: PeUnload(e->module); dlclose(e->handle); */
            if (e + 1 < &g_dll_cache[g_dll_count])
                memmove(e, e + 1,
                        (size_t)(&g_dll_cache[g_dll_count] - (e + 1)) * sizeof(*e));
            g_dll_count--;
            r = STATUS_SUCCESS;
        } else {
            r = STATUS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_dll_lock);
    return r;
}

/* Énumère les DLLs en cache (callback). */
DWORD DllCacheEnum(void (*cb)(const char *name, int refcount, void *ctx),
                   void *ctx)
{
    DWORD i;
    if (!cb) return 0;
    pthread_mutex_lock(&g_dll_lock);
    for (i = 0; i < (DWORD)g_dll_count; i++)
        cb(g_dll_cache[i].name, g_dll_cache[i].refcount, ctx);
    pthread_mutex_unlock(&g_dll_lock);
    return (DWORD)g_dll_count;
}

/* Vide complètement le cache (au shutdown). */
NTSTATUS DllCacheFlush(void)
{
    pthread_mutex_lock(&g_dll_lock);
    g_dll_count = 0;
    memset(g_dll_cache, 0, sizeof(g_dll_cache));
    pthread_mutex_unlock(&g_dll_lock);
    return STATUS_SUCCESS;
}
