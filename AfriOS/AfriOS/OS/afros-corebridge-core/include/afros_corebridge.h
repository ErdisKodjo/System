#ifndef AFROS_COREBRIDGE_H
#define AFROS_COREBRIDGE_H

/**
 * @file afros_corebridge.h
 * @brief Umbrella header for the AfriOS CoreBridge public API (v1.0.0).
 *
 * Include this single header to get the entire Tier 1 / Tier 2 surface:
 *
 *   - Loader API              (loader.h)              — Tier 1 Stable
 *   - Runtime Manager API     (runtime_manager.h)     — Tier 1 Stable
 *   - Version Management API  (version_mgmt.h)        — Tier 2 Beta
 *   - Babel Bridge API        (babelbridge.h)         — Tier 2 Beta
 *   - Orchestrator entry pts  (orchestrator.h)        — Tier 1 Stable
 *
 * The Unified Execution API (VFS, Address Space, Network, Resource
 * Manager) is declared in the corresponding .c files; their public
 * prototypes are re-declared in section 7 of API.md. They are also
 * Tier 1 Stable.
 *
 * Stability contract: the macros and function signatures declared
 * below are FROZEN as of API v1.0.0. Any breaking change requires a
 * new major version bump and a accepted RFC (see RFC-PROCESS.md).
 */

#include "loader.h"
#include "runtime_manager.h"
#include "version_mgmt.h"
#include "babelbridge.h"
#include "orchestrator.h"

/* ------------------------------------------------------------------ */
/* API version                                                        */
/* ------------------------------------------------------------------ */

#define AFROS_COREBRIDGE_API_VERSION_MAJOR 1
#define AFROS_COREBRIDGE_API_VERSION_MINOR 0
#define AFROS_COREBRIDGE_API_VERSION_PATCH 0
#define AFROS_COREBRIDGE_API_VERSION "1.0.0"

/* ------------------------------------------------------------------ */
/* Error codes (returned by all public functions as int / afros_status_t) */
/* ------------------------------------------------------------------ */

#define AFROS_CB_SUCCESS               0
#define AFROS_CB_ERR_INVALID_ARG      (-1)
#define AFROS_CB_ERR_NOT_FOUND        (-2)
#define AFROS_CB_ERR_NO_RUNTIME       (-3)   /* no runtime registered for this app type */
#define AFROS_CB_ERR_OUT_OF_MEMORY    (-4)
#define AFROS_CB_ERR_RUNTIME_CRASHED  (-5)
#define AFROS_CB_ERR_TIMEOUT          (-6)
#define AFROS_CB_ERR_PERMISSION_DENIED (-7)

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* High-level entry points (Tier 1 Stable)                             */
/*                                                                     */
/* Declared in orchestrator.h and re-declared here for visibility.     */
/* The two declarations are guaranteed to match across releases.       */
/* ------------------------------------------------------------------ */

int orchestrator_init(void);
int orchestrator_run_app(const char *path, const char *args);
int orchestrator_shutdown(void);

/* ------------------------------------------------------------------ */
/* API version query (Tier 1 Stable)                                   */
/* ------------------------------------------------------------------ */

const char *afros_corebridge_api_version(void);

#ifdef __cplusplus
}
#endif

#endif /* AFROS_COREBRIDGE_H */
