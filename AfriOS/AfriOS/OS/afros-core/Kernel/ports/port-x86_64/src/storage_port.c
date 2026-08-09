#include "storage_abstraction.h"

/**
 * @file storage_port.c
 * @brief x86_64 Storage operations implementation
 */

static afros_status_t storage_init_impl(void) {
    return AFROS_SUCCESS;
}

static afros_status_t storage_get_info_impl(uint32_t device_id, afros_storage_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    
    // TODO: Get actual storage info from AHCI/NVMe controller
    info->block_size = 512;
    info->block_count = 0; // Unknown until device detected
    info->read_only = false;
    
    return AFROS_SUCCESS;
}

static afros_status_t storage_read_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, uint8_t *buffer) {
    (void)device_id;
    (void)lba;
    (void)count;
    (void)buffer;
    // TODO: Implement AHCI/NVMe read
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t storage_write_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, const uint8_t *buffer) {
    (void)device_id;
    (void)lba;
    (void)count;
    (void)buffer;
    // TODO: Implement AHCI/NVMe write
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t storage_flush_impl(uint32_t device_id) {
    (void)device_id;
    // TODO: Flush cache to physical media
    return AFROS_SUCCESS;
}

storage_ops_t arch_storage_ops = {
    .init = storage_init_impl,
    .get_info = storage_get_info_impl,
    .read_blocks = storage_read_blocks_impl,
    .write_blocks = storage_write_blocks_impl,
    .flush = storage_flush_impl
};
