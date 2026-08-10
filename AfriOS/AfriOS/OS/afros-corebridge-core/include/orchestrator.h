#ifndef AFROS_ORCHESTRATOR_H
#define AFROS_ORCHESTRATOR_H

#include "runtime_manager.h"

/**
 * @file orchestrator.h
 * @brief High-level entry points for AfriOS CoreBridge.
 *
 * These three functions are the only entry points that user-facing
 * tools (afros-launch, the desktop session, third-party launchers)
 * should call. They are **Tier 1 Stable** as of API v1.0.0: any
 * signature change requires a major version bump and a RFC (see
 * `RFC-PROCESS.md`).
 *
 * Implementation lives in `src/api_version.c`; it wires the request
 * through `SelectRuntime()` → `IntelligentLoad()` → `MonitorStart()`.
 * The historical `central_manager.c` stubs have been removed.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the entire CoreBridge subsystem.
 *
 * Idempotent: calling it twice is a no-op. Must be invoked before any
 * other CoreBridge function. Performs:
 *   1. `runtime_init()` — registers built-in runtime managers.
 *   2. `NetStackInit()` — primes the network namespace table.
 *   3. `ResStartMonitor()` — starts the background resource sampler.
 *
 * @return `0` (`AFROS_CB_SUCCESS`) on success, a negative
 *         `AFROS_CB_ERR_*` code on failure.
 */
int orchestrator_init(void);

/**
 * @brief Launch a single application identified by @p path.
 *
 * This is the user-facing entry point. Internally it:
 *   1. Calls `SelectRuntime(path, args)` which in turn runs the
 *      decision engine (`IntelligentLoad`) and consults the
 *      compatibility database.
 *   2. Spawns the chosen runtime with the supplied @p args
 *      (whitespace-separated command-line).
 *   3. Registers the new process with the monitoring subsystem
 *      (`MonitorStart` + `MonitorRegister`) so the watchdog and
 *      resource sampler track it.
 *
 * @param path  Filesystem path to the binary (PE / ELF / Mach-O / DEX
 *              / HAP). Must be non-NULL.
 * @param args  Whitespace-separated command-line arguments, or NULL
 *              for no arguments.
 * @return `0` on success, `AFROS_CB_ERR_NO_RUNTIME` if no compatible
 *         runtime is registered, `AFROS_CB_ERR_NOT_FOUND` if @p path
 *         does not exist, `AFROS_CB_ERR_INVALID_ARG` if @p path is
 *         NULL.
 */
int orchestrator_run_app(const char *path, const char *args);

/**
 * @brief Tear down the CoreBridge subsystem cleanly.
 *
 * Stops the monitor thread, unregisters every runtime, releases all
 * VFS views, address-space regions, network namespaces and resource
 * quotas acquired since `orchestrator_init()`. After this call the
 * library is back in the same state as before initialisation.
 *
 * @return `0` on success, a negative error code on failure.
 */
int orchestrator_shutdown(void);

/**
 * @brief One-shot snapshot of system-wide health.
 *
 * Tier 2 (Beta): the returned struct layout is liable to evolve
 * between minor versions. Not in the umbrella header's frozen set.
 *
 * @return `0` on success.
 */
afros_status_t orchestrator_monitor_system(void);

#ifdef __cplusplus
}
#endif

#endif /* AFROS_ORCHESTRATOR_H */
