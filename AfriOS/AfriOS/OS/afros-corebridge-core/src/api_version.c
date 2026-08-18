/**
 * @file api_version.c
 * @brief Implementation of the Tier 1 high-level orchestrator entry
 *        points plus the API version string.
 *
 * This file replaces the historical stubs that lived in
 * `central_manager.c`. The new implementation wires the user-facing
 * entry points through the real subsystems:
 *
 *   orchestrator_init()
 *       -> runtime_init()        (registers built-in runtime managers)
 *       -> NetStackInit()        (primes the network namespace table)
 *       -> ResStartMonitor()     (background resource sampler thread)
 *
 *   orchestrator_run_app(path, args)
 *       -> SelectRuntime(path, args)
 *              -> IntelligentLoad(path, args)  [decision engine]
 *              -> compat-DB override + runtime_ops_t->initialize()
 *       -> runtime_get_ops(implied)->load_app(path)
 *       -> MonitorStart() + MonitorRegister(handle, pid)
 *
 *   orchestrator_shutdown()
 *       -> MonitorStop()
 *       -> ResStopMonitor()
 *       -> NetStackShutdown()
 *       -> per-runtime Shutdown() (Linux/Win/Android/Ios/Harmony)
 *
 * All public functions return `0` on success and a negative
 * `AFROS_CB_ERR_*` value on failure (see API.md §11).
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/afros_corebridge.h"
#include "../include/loader.h"
#include "../include/runtime_manager.h"
#include "../include/version_mgmt.h"

/* ------------------------------------------------------------------ */
/* API version                                                        */
/* ------------------------------------------------------------------ */

const char *afros_corebridge_api_version(void)
{
    return AFROS_COREBRIDGE_API_VERSION;
}

/* ------------------------------------------------------------------ */
/* Forward declarations of subsystem entry points defined elsewhere    */
/* in the library. Declared here (rather than via a private header)   */
/* so that this TU can be compiled with -fsyntax-only against the     */
/* public headers alone — see verification step in API.md.            */
/* ------------------------------------------------------------------ */

extern afros_status_t NetStackInit(void);
extern afros_status_t NetStackShutdown(void);

extern afros_status_t ResStartMonitor(void);
extern afros_status_t ResStopMonitor(void);

extern afros_status_t MonitorStart(void);
extern afros_status_t MonitorStop(void);
extern afros_status_t MonitorRegister(runtime_handle_t rt, pid_t pid);

extern runtime_handle_t SelectRuntime(const char *path, const char *args);

/* Per-runtime Init/Shutdown functions (weak so the link succeeds even
 * when only a subset of runtime managers is compiled in). */
extern afros_status_t LinuxRuntimeInit(void)    __attribute__((weak));
extern afros_status_t LinuxRuntimeShutdown(void) __attribute__((weak));
extern afros_status_t WinRuntimeInit(void)      __attribute__((weak));
extern afros_status_t WinRuntimeShutdown(void)  __attribute__((weak));
extern afros_status_t AndroidRuntimeInit(void)  __attribute__((weak));
extern afros_status_t AndroidRuntimeShutdown(void) __attribute__((weak));
extern afros_status_t IosRuntimeInit(void)      __attribute__((weak));
extern afros_status_t IosRuntimeShutdown(void)  __attribute__((weak));
extern afros_status_t HarmonyRuntimeInit(void)  __attribute__((weak));
extern afros_status_t HarmonyRuntimeShutdown(void) __attribute__((weak));

/* Per-runtime spawn functions used by orchestrator_run_app. Each
 * runtime manager exposes its own spawn entry point; we call the
 * right one based on the detected app_type_t. */
extern afros_status_t LinuxRuntimeSpawn(const char *path, const char *args,
                                        pid_t *out_pid) __attribute__((weak));
extern afros_status_t WinRuntimeSpawn(const char *path, const char *args,
                                      pid_t *out_pid) __attribute__((weak));
extern afros_status_t AndroidRuntimeSpawnApk(const char *apk_path,
                                             const char *entry,
                                             pid_t *out_pid) __attribute__((weak));
extern afros_status_t IosRuntimeSpawnApp(const char *app_bundle,
                                         const char *entry,
                                         pid_t *out_pid) __attribute__((weak));
extern afros_status_t HarmonyRuntimeSpawnHap(const char *hap_path,
                                             const char *entry,
                                             pid_t *out_pid) __attribute__((weak));

/* Cached detected type for the most-recent SelectRuntime call. We use
 * LoaderCachedType() to recover it from the intelligent_loader cache. */
extern app_type_t LoaderCachedType(const char *path);

/* ------------------------------------------------------------------ */
/* Internal state                                                     */
/* ------------------------------------------------------------------ */

static int g_orchestrator_initialized = 0;

/* ------------------------------------------------------------------ */
/* orchestrator_init                                                  */
/* ------------------------------------------------------------------ */

int orchestrator_init(void)
{
    afros_status_t rc;

    if (g_orchestrator_initialized)
        return 0; /* AFROS_CB_SUCCESS */

    /* 1. Register built-in runtime managers and run their per-module
     *    one-time setup. The per-runtime Init functions are weak; if a
     *    manager is not compiled in, the call is skipped. */
    rc = runtime_init();
    if (rc != AFROS_SUCCESS) {
        return -4; /* AFROS_CB_ERR_NO_RUNTIME */
    }

    if (LinuxRuntimeInit)    (void)LinuxRuntimeInit();
    if (WinRuntimeInit)      (void)WinRuntimeInit();
    if (AndroidRuntimeInit)  (void)AndroidRuntimeInit();
    if (IosRuntimeInit)      (void)IosRuntimeInit();
    if (HarmonyRuntimeInit)  (void)HarmonyRuntimeInit();

    /* 2. Prime the network namespace table. */
    (void)NetStackInit();

    /* 3. Start the background resource sampler. */
    rc = ResStartMonitor();
    if (rc != AFROS_SUCCESS) {
        /* Non-fatal: we can run without quotas, just log. */
        fprintf(stderr,
                "afros-corebridge: ResStartMonitor failed (rc=%u), "
                "continuing without quota enforcement\n",
                (unsigned)rc);
    }

    /* 4. Start the watchdog / heartbeat monitor. */
    rc = MonitorStart();
    if (rc != AFROS_SUCCESS) {
        fprintf(stderr,
                "afros-corebridge: MonitorStart failed (rc=%u), "
                "continuing without watchdog\n",
                (unsigned)rc);
    }

    g_orchestrator_initialized = 1;
    return 0; /* AFROS_CB_SUCCESS */
}

/* ------------------------------------------------------------------ */
/* orchestrator_run_app                                               */
/* ------------------------------------------------------------------ */

int orchestrator_run_app(const char *path, const char *args)
{
    runtime_handle_t handle;
    app_type_t       type;
    pid_t            pid = 0;
    afros_status_t   rc;

    if (!g_orchestrator_initialized)
        return -1; /* AFROS_CB_ERR_INVALID_ARG */

    if (!path)
        return -1; /* AFROS_CB_ERR_INVALID_ARG */

    if (access(path, F_OK) != 0)
        return -3; /* AFROS_CB_ERR_NOT_FOUND */

    /* 1. Decision engine + compat-DB override. SelectRuntime calls
     *    IntelligentLoad internally and initialises the chosen
     *    runtime's ops table. */
    handle = SelectRuntime(path, args);
    if (handle == INVALID_RUNTIME_HANDLE)
        return -5; /* AFROS_CB_ERR_NO_RUNTIME */

    /* 2. Recover the detected app type and dispatch to the matching
     *    runtime spawn function. The spawn function fills @p pid. */
    type = LoaderCachedType(path);

    switch (type) {
    case APP_TYPE_WINDOWS:
        if (!WinRuntimeSpawn) return -5;
        rc = WinRuntimeSpawn(path, args, &pid);
        break;
    case APP_TYPE_ANDROID:
        if (!AndroidRuntimeSpawnApk) return -5;
        rc = AndroidRuntimeSpawnApk(path, args /* entry class */, &pid);
        break;
    case APP_TYPE_MACOS:
        if (!IosRuntimeSpawnApp) return -5;
        rc = IosRuntimeSpawnApp(path, args /* entry */, &pid);
        break;
    case APP_TYPE_HARMONY:
        if (!HarmonyRuntimeSpawnHap) return -5;
        rc = HarmonyRuntimeSpawnHap(path, args /* entry */, &pid);
        break;
    case APP_TYPE_LINUX:
    case APP_TYPE_NATIVE:
    case APP_TYPE_UNKNOWN:
    default:
        if (!LinuxRuntimeSpawn) return -5;
        rc = LinuxRuntimeSpawn(path, args, &pid);
        break;
    }

    if (rc != AFROS_SUCCESS) {
        /* Map AFROS_ERROR_NO_MEMORY (3) to OOM, everything else to
         * runtime crashed / generic failure. */
        if (rc == AFROS_ERROR_NO_MEMORY)
            return -6; /* AFROS_CB_ERR_OUT_OF_MEMORY */
        return -7;     /* AFROS_CB_ERR_RUNTIME_CRASHED */
    }

    /* 3. Register the spawned pid with the monitor so the watchdog
     *    and resource sampler can track it. */
    if (pid > 0) {
        (void)MonitorRegister(handle, pid);
    }

    (void)args;
    return 0; /* AFROS_CB_SUCCESS */
}

/* ------------------------------------------------------------------ */
/* orchestrator_shutdown                                              */
/* ------------------------------------------------------------------ */

int orchestrator_shutdown(void)
{
    if (!g_orchestrator_initialized)
        return 0;

    /* 1. Stop monitor threads (watchdog + resource sampler). */
    (void)MonitorStop();
    (void)ResStopMonitor();

    /* 2. Tear down each runtime manager. */
    if (LinuxRuntimeShutdown)    (void)LinuxRuntimeShutdown();
    if (WinRuntimeShutdown)      (void)WinRuntimeShutdown();
    if (AndroidRuntimeShutdown)  (void)AndroidRuntimeShutdown();
    if (IosRuntimeShutdown)      (void)IosRuntimeShutdown();
    if (HarmonyRuntimeShutdown)  (void)HarmonyRuntimeShutdown();

    /* 3. Tear down the network stack. */
    (void)NetStackShutdown();

    g_orchestrator_initialized = 0;
    return 0;
}
