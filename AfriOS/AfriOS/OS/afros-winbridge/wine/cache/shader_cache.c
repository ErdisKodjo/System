/*
 * shader_cache.c — Cache de shaders HLSL compilés pour afros-winbridge.
 *
 * Délègue la compilation HLSL→SPIR-V à afros-dxvk et maintient un cache
 * persistant sur disque indexé par un hash du source + entry + profile.
 * Évite de recompiler des shaders identiques d'une exécution à l'autre.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#define SHADER_CACHE_DIR "/var/cache/afros-winbridge/shaders"
#define SHADER_MAGIC     0x53414657   /* "WFAS" = Wine Fragment And Shader */
#define MAX_INMEMORY     256

/* --- Entrée de cache mémoire ---------------------------------------- */

typedef struct _SHADER_ENTRY {
    ULONGLONG  key;        /* hash 64-bit du source+entry+profile */
    void      *spirv;      /* bytecode SPIR-V compilé */
    DWORD      size;
    ULONGLONG  last_used;
} SHADER_ENTRY;

static SHADER_ENTRY       g_mem_cache[MAX_INMEMORY];
static int                g_mem_count = 0;
static pthread_mutex_t    g_shader_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- Hash FNV-1a 64-bit (référence) --------------------------------- */

static ULONGLONG fnv1a64(const void *data, DWORD len, ULONGLONG seed)
{
    const BYTE *p = (const BYTE *)data;
    ULONGLONG h = seed ? seed : 0xCBF29CE484222325ULL;
    DWORD i;
    for (i = 0; i < len; i++) {
        h ^= (ULONGLONG)p[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

/* Calcule la clé de cache d'un shader. */
ULONGLONG ShaderCacheComputeKey(const char *src, const char *entry,
                                const char *profile)
{
    ULONGLONG h = 0;
    if (src)     h = fnv1a64(src, (DWORD)strlen(src), h);
    if (entry)   h = fnv1a64(entry, (DWORD)strlen(entry), h);
    if (profile) h = fnv1a64(profile, (DWORD)strlen(profile), h);
    return h;
}

/* Construit le chemin du fichier de cache pour une clé donnée. */
static void shader_cache_path(char *out, size_t sz, ULONGLONG key)
{
    snprintf(out, sz, "%s/%016llx.spv", SHADER_CACHE_DIR,
             (unsigned long long)key);
}

/* --- API publique ------------------------------------------------------ */

/* Recherche un shader dans le cache. Retourne NULL si absent. */
void *ShaderCacheGet(ULONGLONG key, DWORD *size_out)
{
    int i;
    void *result = NULL;
    pthread_mutex_lock(&g_shader_lock);
    /* Cache mémoire d'abord. */
    for (i = 0; i < g_mem_count; i++) {
        if (g_mem_cache[i].key == key) {
            if (size_out) *size_out = g_mem_cache[i].size;
            result = g_mem_cache[i].spirv;
            g_mem_cache[i].last_used = (ULONGLONG)time(NULL) + (ULONGLONG)i;
            pthread_mutex_unlock(&g_shader_lock);
            return result;
        }
    }
    /* Cache disque ensuite. */
    {
        char path[256];
        FILE *f;
        struct stat st;
        void *buf;
        shader_cache_path(path, sizeof(path), key);
        if (stat(path, &st) != 0) {
            pthread_mutex_unlock(&g_shader_lock);
            return NULL;
        }
        f = fopen(path, "rb");
        if (!f) { pthread_mutex_unlock(&g_shader_lock); return NULL; }
        buf = malloc(st.st_size);
        if (!buf) { fclose(f); pthread_mutex_unlock(&g_shader_lock); return NULL; }
        if (fread(buf, 1, st.st_size, f) != (size_t)st.st_size) {
            free(buf); fclose(f); pthread_mutex_unlock(&g_shader_lock);
            return NULL;
        }
        fclose(f);
        if (size_out) *size_out = (DWORD)st.st_size;
        /* Insère dans le cache mémoire. */
        if (g_mem_count < MAX_INMEMORY) {
            g_mem_cache[g_mem_count].key       = key;
            g_mem_cache[g_mem_count].spirv     = buf;
            g_mem_cache[g_mem_count].size      = (DWORD)st.st_size;
            g_mem_cache[g_mem_count].last_used = 0;
            g_mem_count++;
            result = buf;
        } else {
            result = buf; /* caller owns */
        }
    }
    pthread_mutex_unlock(&g_shader_lock);
    return result;
}

/* Insère un shader compilé dans le cache (mémoire + disque). */
NTSTATUS ShaderCachePut(ULONGLONG key, const void *spirv, DWORD size)
{
    int i;
    char path[256];
    FILE *f;
    if (!spirv || size == 0) return STATUS_INVALID_PARAMETER;
    pthread_mutex_lock(&g_shader_lock);
    /* Cache mémoire: remplace si existant. */
    for (i = 0; i < g_mem_count; i++) {
        if (g_mem_cache[i].key == key) {
            free(g_mem_cache[i].spirv);
            g_mem_cache[i].spirv = malloc(size);
            if (!g_mem_cache[i].spirv) {
                pthread_mutex_unlock(&g_shader_lock);
                return STATUS_NO_MEMORY;
            }
            memcpy(g_mem_cache[i].spirv, spirv, size);
            g_mem_cache[i].size = size;
            pthread_mutex_unlock(&g_shader_lock);
            return STATUS_SUCCESS;
        }
    }
    if (g_mem_count < MAX_INMEMORY) {
        g_mem_cache[g_mem_count].key   = key;
        g_mem_cache[g_mem_count].spirv = malloc(size);
        if (!g_mem_cache[g_mem_count].spirv) {
            pthread_mutex_unlock(&g_shader_lock);
            return STATUS_NO_MEMORY;
        }
        memcpy(g_mem_cache[g_mem_count].spirv, spirv, size);
        g_mem_cache[g_mem_count].size = size;
        g_mem_count++;
    }
    /* Cache disque: écriture atomique (tmp + rename). */
    snprintf(path, sizeof(path), "%s/%016llx.spv.tmp", SHADER_CACHE_DIR,
             (unsigned long long)key);
    f = fopen(path, "wb");
    if (f) {
        fwrite(spirv, 1, size, f);
        fclose(f);
        {
            char final[256];
            shader_cache_path(final, sizeof(final), key);
            rename(path, final);
        }
    }
    pthread_mutex_unlock(&g_shader_lock);
    return STATUS_SUCCESS;
}

/* Vide le cache mémoire. */
NTSTATUS ShaderCacheFlush(void)
{
    int i;
    pthread_mutex_lock(&g_shader_lock);
    for (i = 0; i < g_mem_count; i++) {
        free(g_mem_cache[i].spirv);
        g_mem_cache[i].spirv = NULL;
    }
    g_mem_count = 0;
    pthread_mutex_unlock(&g_shader_lock);
    return STATUS_SUCCESS;
}
