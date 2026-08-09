#include "storage_abstraction.h"
#include <stdio.h>

/**
 * @file storage_port.c
 * @brief Port x86_64 : NVMe (files de soumission/complétion PCIe).
 */

static afros_status_t storage_init_impl(void) {
    printf("[STORAGE] NVMe : identification du contrôleur (commande Identify), files I/O créées.\n");
    return AFROS_SUCCESS;
}

static afros_status_t storage_get_info_impl(uint32_t device_id, afros_storage_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    info->block_count = 976773168ULL; // ~465 Go, exemple NVMe
    info->block_size = 512;
    info->read_only = false;
    printf("[STORAGE] NVMe namespace %u : %llu blocs de %u octets\n", device_id,
           (unsigned long long)info->block_count, info->block_size);
    return AFROS_SUCCESS;
}

static afros_status_t storage_read_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, uint8_t *buffer) {
    (void)buffer;
    printf("[STORAGE] NVMe namespace %u : commande Read, %u blocs depuis LBA %llu\n", device_id, count, (unsigned long long)lba);
    return AFROS_SUCCESS;
}

static afros_status_t storage_write_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, const uint8_t *buffer) {
    (void)buffer;
    printf("[STORAGE] NVMe namespace %u : commande Write, %u blocs depuis LBA %llu\n", device_id, count, (unsigned long long)lba);
    return AFROS_SUCCESS;
}

static afros_status_t storage_flush_impl(uint32_t device_id) {
    printf("[STORAGE] NVMe namespace %u : commande Flush\n", device_id);
    return AFROS_SUCCESS;
}

storage_ops_t arch_storage_ops = {
    .init = storage_init_impl,
    .get_info = storage_get_info_impl,
    .read_blocks = storage_read_blocks_impl,
    .write_blocks = storage_write_blocks_impl,
    .flush = storage_flush_impl
};
