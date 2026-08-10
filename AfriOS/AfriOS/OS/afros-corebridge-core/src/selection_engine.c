#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/orchestrator.h"
#include "../include/runtime_manager.h"
#include "../include/loader.h"
#include "../include/version_mgmt.h"

/**
 * @file selection_engine.c
 * @brief The brain of the orchestrator: combines intelligent_loader,
 *        runtime_registry and central_manager to choose the runtime
 *        for a given app.
 *
 * Strategy:
 *   1. Run IntelligentLoad (which detects the format and chooses a
 *      candidate runtime_handle_t).
 *   2. If the path was already cached, return immediately.
 *   3. Otherwise, consult the compatibility matrix at
 *      config/compatibility.db (a small key/value file) to override the
 *      runtime choice if there's an explicit compatibility hint.
 *   4. Verify the chosen runtime is registered and has a default version
 *      installed; if not, try the next-best candidate.
 *   5. Return the chosen handle.
 */

#define COMPAT_DB_PATH "config/compatibility.db"

extern runtime_handle_t LoaderCachedHandle(const char *path);
extern app_type_t       LoaderCachedType(const char *path);

/* Forward declarations of runtime ops getters defined in the runtime_*
 * managers. We declare them here as weak so the selection engine links
 * even when only a subset of runtime managers is compiled in. */
extern const runtime_ops_t *LinuxRuntimeOps(void)    __attribute__((weak));
extern const runtime_ops_t *WinRuntimeOps(void)      __attribute__((weak));
extern const runtime_ops_t *AndroidRuntimeOps(void)  __attribute__((weak));
extern const runtime_ops_t *IosRuntimeOps(void)      __attribute__((weak));
extern const runtime_ops_t *HarmonyRuntimeOps(void)  __attribute__((weak));

/* ------------------------------------------------------------------ */
/* Compatibility DB                                                   */
/* ------------------------------------------------------------------ */

/**
 * The compat DB is a flat key/value file of the form:
 *   <sha256-of-path> = <runtime_type>
 *   <extension>      = <runtime_type>
 *
 * Lines starting with '#' are comments. Lookup is by exact key match.
 */
static afros_runtime_type_t compat_lookup(const char *path)
{
    FILE *fp;
    char line[512];
    afros_runtime_type_t hit = (afros_runtime_type_t)-1;
    const char *db = getenv("AFROS_COMPAT_DB");
    if (!db) db = COMPAT_DB_PATH;

    fp = fopen(db, "r");
    if (!fp) return hit;
    while (fgets(line, sizeof(line), fp)) {
        char *eq, *key, *val;
        long t;
        if (line[0] == '#' || line[0] == '\n') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = line;
        val = eq + 1;
        /* Trim whitespace. */
        while (*key == ' ' || *key == '\t') key++;
        while (*val == ' ' || *val == '\t') val++;
        {
            size_t l = strlen(val);
            while (l > 0 && (val[l-1] == '\n' || val[l-1] == '\r' ||
                              val[l-1] == ' '  || val[l-1] == '\t'))
                val[--l] = '\0';
        }
        /* Match: extension or full path. */
        {
            size_t kp = strlen(key);
            size_t pp = strlen(path);
            if (pp >= kp && strcmp(path + pp - kp, key) == 0) {
                t = strtol(val, NULL, 10);
                hit = (afros_runtime_type_t)t;
                break;
            }
        }
    }
    fclose(fp);
    return hit;
}

/* ------------------------------------------------------------------ */
/* Runtime availability                                               */
/* ------------------------------------------------------------------ */

static int runtime_available(afros_runtime_type_t rt)
{
    version_t v;
    /* If a default version is registered, the runtime is installed. */
    if (VersionGetDefault(rt, &v) == AFROS_SUCCESS)
        return 1;
    /* Otherwise, check whether the ops table is linked in. */
    switch (rt) {
    case RUNTIME_TYPE_LINUX:
        return LinuxRuntimeOps != NULL;
    case RUNTIME_TYPE_WINBRIDGE:
        return WinRuntimeOps != NULL;
    case RUNTIME_TYPE_ANDROID:
        return AndroidRuntimeOps != NULL;
    case RUNTIME_TYPE_IOS:
        return IosRuntimeOps != NULL;
    case RUNTIME_TYPE_HARMONY:
        return HarmonyRuntimeOps != NULL;
    case RUNTIME_TYPE_NATIVE:
        return 1; /* Always available */
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Map a runtime type to its ops table.                               */
/* ------------------------------------------------------------------ */

static const runtime_ops_t *runtime_get_ops(afros_runtime_type_t rt)
{
    switch (rt) {
    case RUNTIME_TYPE_LINUX:     return LinuxRuntimeOps    ? LinuxRuntimeOps()    : NULL;
    case RUNTIME_TYPE_WINBRIDGE: return WinRuntimeOps      ? WinRuntimeOps()      : NULL;
    case RUNTIME_TYPE_ANDROID:   return AndroidRuntimeOps  ? AndroidRuntimeOps()  : NULL;
    case RUNTIME_TYPE_IOS:       return IosRuntimeOps      ? IosRuntimeOps()      : NULL;
    case RUNTIME_TYPE_HARMONY:   return HarmonyRuntimeOps  ? HarmonyRuntimeOps()  : NULL;
    case RUNTIME_TYPE_NATIVE:    return LinuxRuntimeOps    ? LinuxRuntimeOps()    : NULL;
    default: return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

runtime_handle_t SelectRuntime(const char *path, const char *args)
{
    runtime_handle_t handle;
    app_type_t       type;
    afros_runtime_type_t override;
    const runtime_ops_t *ops;

    if (!path)
        return INVALID_RUNTIME_HANDLE;

    /* 1. Run the decision engine. */
    handle = IntelligentLoad(path, args);
    if (handle == INVALID_RUNTIME_HANDLE)
        return INVALID_RUNTIME_HANDLE;

    type = LoaderCachedType(path);

    /* 2. Compatibility DB override. */
    override = compat_lookup(path);
    if ((int)override >= 0 && runtime_available(override)) {
        /* Force this runtime. We still return the same handle (the
         * handle just identifies the launch, not the runtime type) but
         * we ensure the runtime is initialized. */
        ops = runtime_get_ops(override);
        if (ops && ops->initialize)
            ops->initialize();
        return handle;
    }

    /* 3. Make sure the runtime implied by the detected type is
     *    actually available; if not, fall back to native. */
    {
        afros_runtime_type_t implied = RUNTIME_TYPE_NATIVE;
        switch (type) {
        case APP_TYPE_LINUX:   implied = RUNTIME_TYPE_LINUX;     break;
        case APP_TYPE_WINDOWS: implied = RUNTIME_TYPE_WINBRIDGE; break;
        case APP_TYPE_MACOS:   implied = RUNTIME_TYPE_IOS;       break;
        case APP_TYPE_ANDROID: implied = RUNTIME_TYPE_ANDROID;   break;
        case APP_TYPE_HARMONY: implied = RUNTIME_TYPE_HARMONY;   break;
        case APP_TYPE_NATIVE:
        default:               implied = RUNTIME_TYPE_NATIVE;    break;
        }
        if (!runtime_available(implied))
            implied = RUNTIME_TYPE_NATIVE; /* last-ditch fallback */
        ops = runtime_get_ops(implied);
        if (ops && ops->initialize)
            ops->initialize();
    }

    return handle;
}

/* ------------------------------------------------------------------ */
/* Op-table style wrapper                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    runtime_handle_t (*select)(const char *path, const char *args);
    const runtime_ops_t *(*ops_for)(afros_runtime_type_t rt);
    int (*available)(afros_runtime_type_t rt);
} selection_engine_ops_t;

static const selection_engine_ops_t g_engine = {
    .select   = SelectRuntime,
    .ops_for  = runtime_get_ops,
    .available = runtime_available,
};

const selection_engine_ops_t *SelectionEngineOps(void)
{
    return &g_engine;
}
