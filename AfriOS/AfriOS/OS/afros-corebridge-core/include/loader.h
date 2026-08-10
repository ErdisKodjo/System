#ifndef AFROS_COREBRIDGE_LOADER_H
#define AFROS_COREBRIDGE_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include "runtime_manager.h"

/**
 * @file loader.h
 * @brief Application detection, format analysis, dependency resolution and
 *        intelligent runtime selection for AfriOS CoreBridge.
 *
 * This subsystem is the front door of the orchestrator: given a path to a
 * binary (PE / ELF / Mach-O / DEX / HarmonyOS HAP), it detects the format,
 * inspects subsystem/architecture details, resolves shared-library deps,
 * and chooses the best registered runtime manager to spawn it.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Application type detection                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Detected executable type.
 *
 * Order matters: APP_TYPE_NATIVE is the fallback when no magic matches
 * and the file looks like a script or a known AfriOS binary.
 */
typedef enum {
    APP_TYPE_UNKNOWN = 0,   /**< Could not classify                       */
    APP_TYPE_NATIVE,        /**< AfriOS native ELF or script              */
    APP_TYPE_LINUX,         /**< ELF (\x7FELF)                            */
    APP_TYPE_WINDOWS,       /**< PE (MZ)                                  */
    APP_TYPE_MACOS,         /**< Mach-O (0xFEEDFACE / 0xFEEDFACF / ...)   */
    APP_TYPE_ANDROID,       /**< DEX (dex\n035)                            */
    APP_TYPE_HARMONY        /**< HarmonyOS .hap / .hsp (ZIP + module.json)*/
} app_type_t;

/* ------------------------------------------------------------------ */
/* Format analysis                                                    */
/* ------------------------------------------------------------------ */

#define APP_ARCH_UNKNOWN 0
#define APP_ARCH_X86_64  1
#define APP_ARCH_ARM64   2
#define APP_ARCH_RISCV64 3
#define APP_ARCH_X86     4
#define APP_ARCH_ARM32   5

#define PE_SUBSYSTEM_UNKNOWN  0
#define PE_SUBSYSTEM_GUI      2   /* IMAGE_SUBSYSTEM_WINDOWS_GUI         */
#define PE_SUBSYSTEM_CUI      3   /* IMAGE_SUBSYSTEM_WINDOWS_CUI         */

#define MAX_INTERP_LEN     256
#define MAX_DEP_ENTRIES    64
#define MAX_DEP_NAME_LEN   256

/**
 * @brief Detailed information extracted from a binary header.
 */
typedef struct {
    app_type_t  type;                 /**< Result of AppDetect             */
    uint32_t    arch;                 /**< APP_ARCH_*                      */
    uint32_t    subsystem;            /**< PE subsystem (0 for non-PE)     */
    uint32_t    bits;                 /**< 32 or 64                        */
    uint32_t    format_version;       /**< DEX version, PE machine ver, ...*/
    char        interpreter[MAX_INTERP_LEN]; /**< ELF PT_INTERP / ld-linux */
    char        bundle_id[MAX_DEP_NAME_LEN]; /**< macOS / HarmonyOS bundle */
    char        entry_name[MAX_DEP_NAME_LEN];/**< Main class / entry point */
} format_info_t;

/**
 * @brief A single dependency entry (shared lib / dylib / dll / module).
 */
typedef struct {
    char name[MAX_DEP_NAME_LEN];      /**< SONAME / dll name / dylib name  */
    char resolved_path[MAX_DEP_NAME_LEN]; /**< Where it was found (or "")  */
    int  resolved;                    /**< 1 if found, 0 otherwise         */
} dep_entry_t;

/**
 * @brief A resolved dependency list. Heap-allocated; caller frees with
 *        DepListFree().
 */
typedef struct {
    app_type_t  type;
    uint32_t    count;
    dep_entry_t entries[MAX_DEP_ENTRIES];
} dep_list_t;

/* ------------------------------------------------------------------ */
/* Runtime handle                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Opaque handle returned by IntelligentLoad / SelectRuntime.
 *
 * Values are positive integers; 0 means "invalid handle".
 */
typedef uint32_t runtime_handle_t;

#define INVALID_RUNTIME_HANDLE ((runtime_handle_t)0)

/* ------------------------------------------------------------------ */
/* Loader operations table (HAL-style op-table)                       */
/* ------------------------------------------------------------------ */

typedef struct {
    app_type_t      (*detect)(const char *path);
    format_info_t   (*analyze)(const char *path, app_type_t type);
    dep_list_t     *(*resolve_deps)(const char *path, app_type_t type);
    runtime_handle_t (*load)(const char *path, const char *args);
} loader_ops_t;

const loader_ops_t *LoaderGetOps(void);

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/** Detect the application type by reading the file's magic bytes. */
app_type_t AppDetect(const char *path);

/** Detect the application type from an in-memory buffer. */
app_type_t AppDetectBuffer(const void *buf, size_t len);

/** Deep-inspect a binary to populate subsystem / arch / interpreter / etc. */
format_info_t FormatAnalyze(const char *path, app_type_t type);

/** Resolve shared-library dependencies for the given binary. */
dep_list_t *ResolveDeps(const char *path, app_type_t type);

/** Free a dep_list_t previously returned by ResolveDeps. */
void DepListFree(dep_list_t *list);

/**
 * @brief Decision engine: detect + analyze + consult runtime registry +
 *        central manager, pick the best runtime, cache the decision.
 * @returns a valid runtime_handle_t or INVALID_RUNTIME_HANDLE on failure.
 */
runtime_handle_t IntelligentLoad(const char *path, const char *args);

#ifdef __cplusplus
}
#endif

#endif /* AFROS_COREBRIDGE_LOADER_H */
