#ifndef AFROS_RUNTIME_MANAGER_H
#define AFROS_RUNTIME_MANAGER_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file runtime_manager.h
 * @brief Multi-platform runtime management interface for AfriOS.
 */

typedef enum {
    RUNTIME_TYPE_NATIVE,
    RUNTIME_TYPE_ANDROID,
    RUNTIME_TYPE_LINUX,
    RUNTIME_TYPE_WINBRIDGE,
    RUNTIME_TYPE_IOS,
    RUNTIME_TYPE_HARMONY
} afros_runtime_type_t;

typedef struct {
    afros_status_t (*initialize)(void);
    afros_status_t (*load_app)(const char *path);
    afros_status_t (*start_app)(const char *name);
    afros_status_t (*stop_app)(const char *name);
    afros_status_t (*get_status)(const char *name, uint32_t *status);
} runtime_ops_t;

afros_status_t runtime_register_manager(afros_runtime_type_t type, runtime_ops_t *ops);
afros_status_t runtime_init(void);

#endif
