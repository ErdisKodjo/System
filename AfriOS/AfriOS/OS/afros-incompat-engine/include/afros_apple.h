#ifndef AFROS_APPLE_COMPAT_H
#define AFROS_APPLE_COMPAT_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file afros_apple.h
 * @brief Apple/iOS compatibility layer (macho-loader) for AfriOS.
 */

typedef struct {
    afros_status_t (*load_macho)(const char *path);
    afros_status_t (*emulate_dyld)(void);
    afros_status_t (*resolve_symbols)(void);
} apple_compat_ops_t;

afros_status_t apple_compat_init(void);
afros_status_t apple_launch_macho(const char *path);

#endif
