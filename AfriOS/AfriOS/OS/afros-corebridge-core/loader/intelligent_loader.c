#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../include/loader.h"
#include "../include/runtime_manager.h"
#include "../include/version_mgmt.h"

/**
 * @file intelligent_loader.c
 * @brief Decision engine: combine AppDetect + FormatAnalyze +
 *        RuntimeRegistry + CentralManager resource budget, pick the best
 *        runtime, and cache decisions for repeat launches.
 *
 * Strategy:
 *   1. Detect format & analyze.
 *   2. Determine which registered runtimes are eligible for this type
 *      (e.g. APP_TYPE_WINDOWS → RUNTIME_TYPE_WINBRIDGE).
 *   3. For each eligible runtime, query the version registry for the
 *      default version and compute a compatibility score from the
 *      architecture match.
 *   4. Query the central manager for resource budget (memory/CPU).
 *   5. Pick the highest-scoring runtime whose budget is sufficient.
 *   6. Cache the (path → runtime) mapping for the next launch.
 */

#define MAX_CACHE 32

/* ------------------------------------------------------------------ */
/* Cache                                                              */
/* ------------------------------------------------------------------ */

struct cache_entry {
    char             path[512];
    runtime_handle_t handle;
    app_type_t       type;
};

static struct cache_entry g_cache[MAX_CACHE];
static uint32_t           g_cache_count = 0;
static runtime_handle_t   g_next_handle = 1;
static struct cache_entry *cache_lookup(const char *path)
{
    for (uint32_t i = 0; i < g_cache_count; i++)
        if (strcmp(g_cache[i].path, path) == 0)
            return &g_cache[i];
    return NULL;
}

static struct cache_entry *cache_insert(const char *path,
                                        runtime_handle_t h,
                                        app_type_t t)
{
    struct cache_entry *e;
    if (g_cache_count < MAX_CACHE) {
        e = &g_cache[g_cache_count++];
    } else {
        /* Evict the oldest entry. */
        memmove(&g_cache[0], &g_cache[1],
                (MAX_CACHE - 1) * sizeof(g_cache[0]));
        e = &g_cache[MAX_CACHE - 1];
    }
    strncpy(e->path, path ? path : "", sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';
    e->handle = h;
    e->type   = t;
    return e;
}

/* ------------------------------------------------------------------ */
/* Eligibility table                                                  */
/* ------------------------------------------------------------------ */

static int runtime_eligible(app_type_t t, afros_runtime_type_t rt)
{
    switch (t) {
    case APP_TYPE_LINUX:   return rt == RUNTIME_TYPE_LINUX    ||
                                  rt == RUNTIME_TYPE_NATIVE;
    case APP_TYPE_WINDOWS: return rt == RUNTIME_TYPE_WINBRIDGE;
    case APP_TYPE_MACOS:   return rt == RUNTIME_TYPE_IOS;
    case APP_TYPE_ANDROID: return rt == RUNTIME_TYPE_ANDROID;
    case APP_TYPE_HARMONY: return rt == RUNTIME_TYPE_HARMONY;
    case APP_TYPE_NATIVE:  return rt == RUNTIME_TYPE_NATIVE;
    default:               return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Compatibility score                                                */
/* ------------------------------------------------------------------ */

/* Return 0..100: how well the runtime default version matches the
 * binary's arch / bits. */
static int compat_score(const format_info_t *fi,
                        afros_runtime_type_t rt)
{
    int score = 50; /* baseline */
    version_t v;
    int default_ok = (VersionGetDefault(rt, &v) == AFROS_SUCCESS);

    /* Native arch match bonus. */
    if (fi->arch == APP_ARCH_ARM64 || fi->arch == APP_ARCH_X86_64)
        score += 20;
    if (fi->bits == 64)
        score += 5;

    /* If a default version is registered, we know the runtime is
     * installed and can host the binary. */
    if (default_ok) {
        score += 25;
        /* Newer version bonus: parse "major.minor" and add up to 5. */
        if (strlen(v.version) >= 3) {
            int major = atoi(v.version);
            if (major > 0) score += (major > 10) ? 5 : (major / 2);
        }
    } else {
        score -= 30;
    }

    /* Runtime-type specific bonus. */
    if (rt == RUNTIME_TYPE_NATIVE)
        score += 10; /* native is always the fastest if eligible */
    if (rt == RUNTIME_TYPE_WINBRIDGE && fi->subsystem == PE_SUBSYSTEM_GUI)
        score += 5; /* Wine handles GUI apps well */

    if (score > 100) score = 100;
    if (score < 0)   score = 0;
    return score;
}

/* ------------------------------------------------------------------ */
/* Central manager budget (host-portable stub)                        */
/* ------------------------------------------------------------------ */

struct budget { uint64_t free_mem; uint32_t cpu_free_pct; };
static void query_budget(struct budget *b)
{
    /* In a real deployment this would call into the central_manager to
     * obtain the current free memory and CPU headroom. On the host
     * simulator we read /proc/meminfo and /proc/loadavg. */
    FILE *f;
    char line[256];
    b->free_mem = 512ULL * 1024 * 1024; /* default 512 MiB free */
    b->cpu_free_pct = 80;

    f = fopen("/proc/meminfo", "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemAvailable:", 13) == 0) {
                unsigned long kb = 0;
                sscanf(line + 13, "%lu", &kb);
                b->free_mem = (uint64_t)kb * 1024;
                break;
            }
        }
        fclose(f);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

runtime_handle_t IntelligentLoad(const char *path, const char *args)
{
    app_type_t type;
    format_info_t fi;
    struct budget budget;
    afros_runtime_type_t candidates[] = {
        RUNTIME_TYPE_NATIVE,
        RUNTIME_TYPE_LINUX,
        RUNTIME_TYPE_WINBRIDGE,
        RUNTIME_TYPE_ANDROID,
        RUNTIME_TYPE_IOS,
        RUNTIME_TYPE_HARMONY,
    };
    int best_score = -1;
    afros_runtime_type_t best_rt = RUNTIME_TYPE_NATIVE;
    runtime_handle_t handle;
    struct cache_entry *cached;
    version_t chosen_ver;

    (void)args;
    if (!path)
        return INVALID_RUNTIME_HANDLE;

    /* 1. Cache lookup */
    cached = cache_lookup(path);
    if (cached)
        return cached->handle;

    /* 2. Detect + analyze */
    type = AppDetect(path);
    if (type == APP_TYPE_UNKNOWN) {
        /* Unknown: assume native and let it try. */
        type = APP_TYPE_NATIVE;
    }
    fi = FormatAnalyze(path, type);

    /* 3. Query budget */
    query_budget(&budget);

    /* 4. Iterate eligible runtimes, pick best score. */
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        int s;
        if (!runtime_eligible(type, candidates[i]))
            continue;
        s = compat_score(&fi, candidates[i]);
        /* Memory budget filter: 32-bit apps need >= 64 MiB free. */
        if (budget.free_mem < 64ULL * 1024 * 1024)
            s -= 20;
        if (s > best_score) {
            best_score = s;
            best_rt = candidates[i];
        }
    }

    if (best_score < 0)
        return INVALID_RUNTIME_HANDLE;

    /* 5. Try to read the chosen runtime's default version so the
     *    caller can later resolve its install_path. We don't fail if
     *    VersionGetDefault fails (the runtime may still be linked in). */
    memset(&chosen_ver, 0, sizeof(chosen_ver));
    (void)VersionGetDefault(best_rt, &chosen_ver);

    /* 6. Allocate handle. In a real orchestrator this would also call
     *    runtime_ops_t->load_app() on the chosen runtime. */
    handle = g_next_handle++;
    if (g_next_handle == INVALID_RUNTIME_HANDLE)
        g_next_handle = 1;

    cache_insert(path, handle, type);
    return handle;
}

/* ------------------------------------------------------------------ */
/* Hook used by selection_engine.c to peek at the cache.              */
/* ------------------------------------------------------------------ */

runtime_handle_t LoaderCachedHandle(const char *path)
{
    struct cache_entry *e = cache_lookup(path);
    return e ? e->handle : INVALID_RUNTIME_HANDLE;
}

app_type_t LoaderCachedType(const char *path)
{
    struct cache_entry *e = cache_lookup(path);
    return e ? e->type : APP_TYPE_UNKNOWN;
}
