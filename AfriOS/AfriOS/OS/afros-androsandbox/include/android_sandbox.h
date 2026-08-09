#ifndef AFROS_ANDROID_SANDBOX_H
#define AFROS_ANDROID_SANDBOX_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file android_sandbox.h
 * @brief Moteur d'ex�cution d'applications Android pour AfriOS.
 */

typedef struct {
    afros_status_t (*start_vm)(void);
    afros_status_t (*load_apk)(const char *apk_path);
    afros_status_t (*map_binder_service)(const char *service_name);
} android_ops_t;

typedef struct {
    uint32_t handle;
    void *cookie;
} binder_ptr_t;

typedef struct {
    uint32_t code;
    const uint8_t *data;
    size_t data_size;
} binder_transaction_t;

afros_status_t binder_init(void);
afros_status_t binder_transaction(uint32_t handle, binder_transaction_t *txn);

afros_status_t android_sandbox_init(void);

#endif
