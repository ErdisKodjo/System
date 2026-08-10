#include "storage_abstraction.h"
#include "kprintf.h"

/**
 * @file storage_port.c
 * @brief Port MCU : flash NOR externe en SPI (souvent en lecture seule / XIP
 *        pour le code, une petite zone en écriture pour les données/logs).
 */

static afros_status_t storage_init_impl(void) {
    kprintf("[STORAGE] MCU : sonde flash SPI NOR (commande JEDEC ID 0x9F).\n");
    return AFROS_SUCCESS;
}

static afros_status_t storage_get_info_impl(uint32_t device_id, afros_storage_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    if (device_id != 0) return AFROS_ERROR_INVALID_PARAM; // un seul device flash sur ce port
    info->block_count = 4096; // 4096 secteurs
    info->block_size = 4096;  // secteurs de 4 Ko (taille d'effacement typique)
    info->read_only = true;   // XIP : zone code en lecture seule par défaut
    kprintf("[STORAGE] SPI NOR : %llu secteurs de %u octets (lecture seule)\n",
           (unsigned long long)info->block_count, info->block_size);
    return AFROS_SUCCESS;
}

static afros_status_t storage_read_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, uint8_t *buffer) {
    (void)buffer;
    if (device_id != 0) return AFROS_ERROR_INVALID_PARAM;
    kprintf("[STORAGE] SPI NOR : lecture de %u secteurs depuis 0x%llx (commande READ 0x03/0x0B).\n",
           count, (unsigned long long)lba);
    return AFROS_SUCCESS;
}

static afros_status_t storage_write_blocks_impl(uint32_t device_id, uint64_t lba, uint32_t count, const uint8_t *buffer) {
    (void)device_id; (void)lba; (void)count; (void)buffer;
    // Zone code en lecture seule sur ce port ; l'écriture nécessiterait un
    // effacement de secteur préalable (0x20) réservé à la zone data.
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t storage_flush_impl(uint32_t device_id) {
    (void)device_id;
    return AFROS_SUCCESS; // pas de cache d'écriture sur ce port
}

storage_ops_t arch_storage_ops = {
    .init = storage_init_impl,
    .get_info = storage_get_info_impl,
    .read_blocks = storage_read_blocks_impl,
    .write_blocks = storage_write_blocks_impl,
    .flush = storage_flush_impl
};
