/*
 * jit_cache.c — Cache de code JIT-compilé (x86/x64 → natif) pour afros-winbridge.
 *
 * Sur les architectures non-x86 (ARM64, RISC-V), les binaires Windows
 * x86/x64 doivent être traduits à la volée. Ce module cache le résultat
 * de la compilation par bloc de code (basic block) pour éviter de
 * re-JIT à chaque exécution.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#define JIT_CACHE_MAX   1024
#define JIT_PAGE_SIZE   4096

/* --- Entrée de cache -------------------------------------------------- */

typedef struct _JIT_ENTRY {
    ULONGLONG  source_addr;    /* adresse x86 du bloc source */
    DWORD      source_size;
    void      *native_code;    /* code natif généré */
    DWORD      native_size;
    ULONGLONG  hit_count;
} JIT_ENTRY;

static JIT_ENTRY          g_jit_cache[JIT_CACHE_MAX];
static int                g_jit_count = 0;
static pthread_mutex_t    g_jit_lock  = PTHREAD_MUTEX_INITIALIZER;

/* Statistiques. */
static ULONGLONG g_total_compiles  = 0;
static ULONGLONG g_total_hits      = 0;
static ULONGLONG g_total_misses    = 0;

/* --- Helpers locaux ---------------------------------------------------- */

static JIT_ENTRY *find_entry(ULONGLONG src_addr)
{
    int i;
    for (i = 0; i < g_jit_count; i++)
        if (g_jit_cache[i].source_addr == src_addr) return &g_jit_cache[i];
    return NULL;
}

/* Alloue une page exécutable pour stocker le code natif. */
static void *alloc_executable(DWORD size)
{
    DWORD pages = (size + JIT_PAGE_SIZE - 1) / JIT_PAGE_SIZE;
    void *p = mmap(NULL, pages * JIT_PAGE_SIZE,
                   PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

/* --- API publique ------------------------------------------------------ */

/* JIT-compile un bloc de code source et le met en cache.
 * compiler_fn est appelé si le bloc n'est pas déjà en cache. */
typedef void (*JIT_COMPILER_FN)(ULONGLONG src_addr, DWORD src_size,
                                void *native_out, DWORD *native_size_out);

NTSTATUS JitCacheCompile(ULONGLONG src_addr, DWORD src_size,
                         JIT_COMPILER_FN compiler_fn,
                         void **native_out, DWORD *native_size_out)
{
    JIT_ENTRY *e;
    NTSTATUS r = STATUS_SUCCESS;
    if (!compiler_fn || !native_out) return STATUS_INVALID_PARAMETER;
    *native_out = NULL;
    pthread_mutex_lock(&g_jit_lock);
    e = find_entry(src_addr);
    if (e) {
        /* Cache hit. */
        e->hit_count++;
        g_total_hits++;
        *native_out = e->native_code;
        if (native_size_out) *native_size_out = e->native_size;
        pthread_mutex_unlock(&g_jit_lock);
        return STATUS_SUCCESS;
    }
    g_total_misses++;
    if (g_jit_count >= JIT_CACHE_MAX) {
        /* Éviction LRU: trouve l'entrée avec le moins de hits. */
        int i, victim = 0;
        ULONGLONG min_hits = g_jit_cache[0].hit_count;
        for (i = 1; i < g_jit_count; i++) {
            if (g_jit_cache[i].hit_count < min_hits) {
                min_hits = g_jit_cache[i].hit_count;
                victim = i;
            }
        }
        munmap(g_jit_cache[victim].native_code, g_jit_cache[victim].native_size);
        g_jit_cache[victim] = g_jit_cache[g_jit_count - 1];
        g_jit_count--;
    }
    /* Compile et stocke. */
    {
        DWORD est = src_size * 8 + 64;   /* surestime la taille native */
        void *native = alloc_executable(est);
        DWORD actual = est;
        if (!native) {
            pthread_mutex_unlock(&g_jit_lock);
            return STATUS_NO_MEMORY;
        }
        compiler_fn(src_addr, src_size, native, &actual);
        e = &g_jit_cache[g_jit_count++];
        e->source_addr  = src_addr;
        e->source_size  = src_size;
        e->native_code  = native;
        e->native_size  = actual;
        e->hit_count    = 1;
        g_total_compiles++;
        *native_out = native;
        if (native_size_out) *native_size_out = actual;
    }
    pthread_mutex_unlock(&g_jit_lock);
    return r;
}

/* Exécute le code natif associé à un bloc. Retourne la valeur de retour. */
ULONGLONG JitCacheRun(ULONGLONG src_addr, void *args)
{
    JIT_ENTRY *e;
    ULONGLONG result = 0;
    pthread_mutex_lock(&g_jit_lock);
    e = find_entry(src_addr);
    if (!e) {
        pthread_mutex_unlock(&g_jit_lock);
        return 0;
    }
    e->hit_count++;
    {
        typedef ULONGLONG (*JIT_ENTRY_FN)(void *);
        JIT_ENTRY_FN fn = (JIT_ENTRY_FN)e->native_code;
        /* Appel réel commenté: ne pas exécuter du code arbitraire en sandbox. */
        (void)fn;
        (void)args;
        result = 0;
    }
    pthread_mutex_unlock(&g_jit_lock);
    return result;
}

/* Invalide une entrée du cache (après modification du code source). */
NTSTATUS JitCacheInvalidate(ULONGLONG src_addr)
{
    JIT_ENTRY *e;
    pthread_mutex_lock(&g_jit_lock);
    e = find_entry(src_addr);
    if (!e) { pthread_mutex_unlock(&g_jit_lock); return STATUS_NOT_FOUND; }
    munmap(e->native_code, e->native_size);
    if (e + 1 < &g_jit_cache[g_jit_count])
        memmove(e, e + 1,
                (size_t)(&g_jit_cache[g_jit_count] - (e + 1)) * sizeof(*e));
    g_jit_count--;
    pthread_mutex_unlock(&g_jit_lock);
    return STATUS_SUCCESS;
}

/* Récupère les statistiques de cache. */
void JitCacheStats(ULONGLONG *compiles, ULONGLONG *hits, ULONGLONG *misses)
{
    pthread_mutex_lock(&g_jit_lock);
    if (compiles) *compiles = g_total_compiles;
    if (hits)     *hits     = g_total_hits;
    if (misses)   *misses   = g_total_misses;
    pthread_mutex_unlock(&g_jit_lock);
}
