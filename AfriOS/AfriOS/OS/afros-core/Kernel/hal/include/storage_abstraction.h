#ifndef STORAGE_ABSTRACTION_H
#define STORAGE_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file storage_abstraction.h
 * @brief Block storage abstraction for AfriOS (eMMC / NVMe / virtio-blk / SD/SPI-flash
 *        on MCU targets). AfrosFS (Kernel/afros/fs/afrosfs/) sits on top of this, never
 *        talks to a controller directly.
 */

typedef struct {
    uint64_t block_count;
    uint32_t block_size;
    bool     read_only;
} afros_storage_info_t;

typedef struct {
    afros_status_t (*init)(void);
    afros_status_t (*get_info)(uint32_t device_id, afros_storage_info_t *info);
    afros_status_t (*read_blocks)(uint32_t device_id, uint64_t lba, uint32_t count, uint8_t *buffer);
    afros_status_t (*write_blocks)(uint32_t device_id, uint64_t lba, uint32_t count, const uint8_t *buffer);
    afros_status_t (*flush)(uint32_t device_id);
} storage_ops_t;

extern storage_ops_t arch_storage_ops;

#endif // STORAGE_ABSTRACTION_H
