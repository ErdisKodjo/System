#include "afros_hal.h"
#include <stdio.h>

/**
 * @file device_manager.c
 * @brief Registre générique de périphériques pour AfriOS (étape 4 — gap
 *        signalé à l'étape 2/3 : device_abstraction.h n'avait aucune
 *        implémentation, aucun driver ne pouvait s'enregistrer réellement).
 *
 * Générique et indépendant de l'architecture : la bibliothèque tient juste
 * un registre de device_ops_t*, elle ne touche jamais le matériel elle-même
 * (c'est le rôle de chaque driver dans Kernel/drivers/).
 */

#define AFROS_MAX_DEVICES 32

static device_ops_t *s_devices[AFROS_MAX_DEVICES];
static uint32_t s_device_count = 0;

static afros_status_t device_manager_register_impl(device_ops_t *ops) {
    if (!ops) return AFROS_ERROR_INVALID_PARAM;

    for (uint32_t i = 0; i < s_device_count; i++) {
        if (s_devices[i]->device_id == ops->device_id) {
            // Device already registered, refuse
            return AFROS_ERROR;
        }
    }
    if (s_device_count >= AFROS_MAX_DEVICES) {
        // Registry full
        return AFROS_ERROR_NO_MEMORY;
    }

    s_devices[s_device_count++] = ops;

    if (ops->init) {
        return ops->init(ops->device_id);
    }
    return AFROS_SUCCESS;
}

static afros_status_t device_manager_unregister_impl(uint32_t device_id) {
    for (uint32_t i = 0; i < s_device_count; i++) {
        if (s_devices[i]->device_id == device_id) {
            // Device removed
            s_devices[i] = s_devices[s_device_count - 1];
            s_device_count--;
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR_INVALID_PARAM;
}

device_manager_ops_t arch_device_manager_ops = {
    .register_device = device_manager_register_impl,
    .unregister_device = device_manager_unregister_impl
};
