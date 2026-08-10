#ifndef AFROS_COREBRIDGE_VERSION_MGMT_H
#define AFROS_COREBRIDGE_VERSION_MGMT_H

#include <stdint.h>
#include <stddef.h>
#include "runtime_manager.h"

/**
 * @file version_mgmt.h
 * @brief Runtime version registry, update checking, downloading and
 *        installation for AfriOS CoreBridge.
 *
 * Version management keeps a JSON registry of every installed runtime
 * (Linux/Wine/Android/Darling/HarmonyGate), lets the orchestrator know
 * which one is the default for each runtime type, and supports polling a
 * remote manifest for newer versions, downloading them and installing
 * them with rollback support.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Type aliases                                                       */
/* ------------------------------------------------------------------ */

/** Reuse the runtime type enum from runtime_manager.h. */
typedef afros_runtime_type_t runtime_type_t;

#define MAX_VERSION_STR   64
#define MAX_INSTALL_PATH 512

/* ------------------------------------------------------------------ */
/* Version                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief A registered runtime version (e.g. Wine 9.0, Darling 0.1.32).
 */
typedef struct {
    runtime_type_t type;                 /**< RUNTIME_TYPE_*              */
    char           version[MAX_VERSION_STR]; /**< "9.0"                   */
    char           install_path[MAX_INSTALL_PATH]; /**< /opt/afros/wine/9.0 */
    int            is_default;           /**< 1 if default for type       */
} version_t;

/* ------------------------------------------------------------------ */
/* Quota / usage                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Resource quota for a runtime (CPU %, memory, IO budget).
 */
typedef struct {
    uint32_t cpu_weight;     /**< 1..1000 (cgroup-style shares)         */
    uint64_t mem_limit_bytes;/**< Max RSS in bytes (0 = unlimited)      */
    uint64_t io_quota_kbps;  /**< Max read+write KB/s                   */
    uint32_t fd_limit;       /**< Max open file descriptors             */
    uint32_t port_limit;     /**< Max bound TCP/UDP ports               */
} quota_t;

/**
 * @brief Current resource usage of a runtime.
 */
typedef struct {
    uint32_t cpu_percent;    /**< 0..100                                 */
    uint64_t mem_used_bytes; /**< Current RSS                            */
    uint64_t io_read_kb;     /**< Cumulative KB read                     */
    uint64_t io_write_kb;    /**< Cumulative KB written                  */
    uint32_t fd_count;       /**< Open file descriptors                  */
    uint32_t port_count;     /**< Bound ports                            */
    uint64_t faults;         /**< Page faults / crashes                  */
} usage_t;

/* ------------------------------------------------------------------ */
/* Version registry                                                   */
/* ------------------------------------------------------------------ */

/** Register or update a runtime version. Returns AFROS_SUCCESS on success. */
afros_status_t VersionRegister(const version_t *v);

/** Remove a runtime version from the registry. */
afros_status_t VersionUnregister(runtime_type_t type, const char *version);

/** List all registered versions. Caller provides an array of @p max entries. */
uint32_t VersionList(version_t *out, uint32_t max);

/** Get the default version for a runtime type. */
afros_status_t VersionGetDefault(runtime_type_t type, version_t *out);

/** Set the default version for a runtime type. */
afros_status_t VersionSetDefault(runtime_type_t type, const char *version);

/* ------------------------------------------------------------------ */
/* Update checker                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Check the remote manifest for the latest version of @p rt.
 * @returns AFROS_SUCCESS and fills @p latest, or AFROS_ERROR_* on failure.
 */
afros_status_t UpdateCheck(runtime_type_t rt, version_t *latest);

/** Check all runtime types; returns the number of updates available. */
uint32_t UpdateCheckAll(version_t *updates, uint32_t max);

/* ------------------------------------------------------------------ */
/* Downloader                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Download a runtime archive, verifying SHA-256.
 * @param url             HTTPS URL to fetch.
 * @param dest_path       Local file path to write to.
 * @param expected_sha256 Hex SHA-256 (64 chars) or NULL to skip verify.
 * @returns AFROS_SUCCESS on success.
 */
afros_status_t DownloaderFetch(const char *url,
                               const char *dest_path,
                               const char *expected_sha256);

/* ------------------------------------------------------------------ */
/* Installer                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Extract a downloaded archive, register it via VersionRegister,
 *        and validate by running the runtime's self-test.
 */
afros_status_t InstallerInstall(const char *archive_path);

/** Roll back to the previous default version for a runtime type. */
afros_status_t InstallerRollback(runtime_type_t rt);

/* ------------------------------------------------------------------ */
/* Op table                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    afros_status_t (*register_ver)(const version_t *);
    afros_status_t (*unregister)(runtime_type_t, const char *);
    uint32_t       (*list)(version_t *, uint32_t);
    afros_status_t (*get_default)(runtime_type_t, version_t *);
    afros_status_t (*set_default)(runtime_type_t, const char *);
    afros_status_t (*check)(runtime_type_t, version_t *);
    afros_status_t (*fetch)(const char *, const char *, const char *);
    afros_status_t (*install)(const char *);
    afros_status_t (*rollback)(runtime_type_t);
} version_mgmt_ops_t;

const version_mgmt_ops_t *VersionMgmtGetOps(void);

#ifdef __cplusplus
}
#endif

#endif /* AFROS_COREBRIDGE_VERSION_MGMT_H */
