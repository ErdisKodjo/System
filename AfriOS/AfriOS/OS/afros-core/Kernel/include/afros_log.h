#ifndef AFROS_LOG_H
#define AFROS_LOG_H

/**
 * @file afros_log.h
 * @brief Logging primitives for AfriOS kernel modules.
 *
 * Provides `afros_log_info`, `afros_log_warning`, `afros_log_error`,
 * `afros_log_debug` macros that route through `kprintf` (the freestanding-safe
 * formatter) when `AFROS_FREESTANDING` is defined, or via `printf` on host
 * builds. Modules can opt-in via `#include "afros_log.h"` — no extra link
 * dependency required.
 */

#include "afros_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* When the kernel is freestanding, route through kprintf (declared in
 * hal/include/kprintf.h). On host builds, fall back to printf from libc. */
#ifdef AFROS_FREESTANDING
    #include "kprintf.h"
    #define AFROS_LOG_IMPL kprintf
#else
    #include <stdio.h>
    #define AFROS_LOG_IMPL printf
#endif

#define afros_log_info(fmt, ...)    AFROS_LOG_IMPL("[INFO ] " fmt, ##__VA_ARGS__)
#define afros_log_warning(fmt, ...) AFROS_LOG_IMPL("[WARN ] " fmt, ##__VA_ARGS__)
#define afros_log_error(fmt, ...)   AFROS_LOG_IMPL("[ERROR] " fmt, ##__VA_ARGS__)
#define afros_log_debug(fmt, ...)   AFROS_LOG_IMPL("[DEBUG] " fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AFROS_LOG_H */
