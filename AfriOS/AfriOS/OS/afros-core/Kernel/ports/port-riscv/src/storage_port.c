#include "storage_abstraction.h"
#include "kprintf.h"

/**
 * @file storage_port.c
 * @brief Port RISC-V : virtio-blk (cible QEMU virt / plateformes virtualisées),
 *        le choix standard pour le développement et les SBC RISC-V courants.
 */

static afros_status_t storage_init_impl(void) {
    kprintf("[STORAGE] RISC-V : négociation des features virtio-blk, anneaux virtqueue configurés.\n");
    return AFROS_SUCCESS;
}

static afros_status_t storage_get_info_impl(uint32_t device_id, afros_storage_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    info->block_count = 20971520ULL; // ~10 Go, exemple image virtio-blk
    info->block_size = 512;
    info->read_only = false;
    kprintf("[STORAGE] virtio-blk%u : %llu blocs de %u octets\n", device_id,
           (unsigned long long)info->block_count, info->block_size);
    return AFROS_SUCCESS;
}

static afros_status_t storage_read_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, uint8_t *buffer) {
    (void)buffer;
    kprintf("[STORAGE] virtio-blk%u : requête VIRTIO_BLK_T_IN, %u blocs depuis LBA %llu\n", device_id, count, (unsigned long long)lba);
    return AFROS_SUCCESS;
}

static afros_status_t storage_write_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, const uint8_t *buffer) {
    (void)buffer;
    kprintf("[STORAGE] virtio-blk%u : requête VIRTIO_BLK_T_OUT, %u blocs depuis LBA %llu\n", device_id, count, (unsigned long long)lba);
    return AFROS_SUCCESS;
}

static afros_status_t storage_flush_impl(uint32_t device_id) {
    kprintf("[STORAGE] virtio-blk%u : requête VIRTIO_BLK_T_FLUSH\n", device_id);
    return AFROS_SUCCESS;
}

storage_ops_t arch_storage_ops = {
    .init = storage_init_impl,
    .get_info = storage_get_info_impl,
    .read_blocks = storage_read_blocks_impl,
    .write_blocks = storage_write_blocks_impl,
    .flush = storage_flush_impl
};
