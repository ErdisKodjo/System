#ifndef DEVICE_ABSTRACTION_H
#define DEVICE_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file device_abstraction.h
 * @brief Device abstraction layer for AfriOS.
 */

typedef struct {
    uint32_t device_id;
    const char *name;
    afros_status_t (*init)(uint32_t device_id);
    afros_status_t (*read)(uint32_t device_id, uint8_t *buffer, afros_size_t size);
    afros_status_t (*write)(uint32_t device_id, const uint8_t *buffer, afros_size_t size);
    afros_status_t (*ioctl)(uint32_t device_id, uint32_t command, void *args);
} device_ops_t;

typedef struct {
    afros_status_t (*register_device)(device_ops_t *ops);
    afros_status_t (*unregister_device)(uint32_t device_id);
} device_manager_ops_t;

extern device_manager_ops_t arch_device_manager_ops;

#endif // DEVICE_ABSTRACTION_H
