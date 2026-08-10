#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file compression.c
 * @brief Gestionnaire de compression mémoire (ZRAM) pour AfriOS.
 */

void memory_compress_task_data(uint32_t task_id, afros_virt_addr_t data_addr, afros_size_t size) {
    kprintf("[MEM] Compression demandée pour la tâche %u à l'adresse 0x%llx (%zu octets)...\n", task_id, data_addr, size);
    
    // 1. Appel au matériel de compression via le HAL
    if (arch_memory_ops.compress(data_addr, size) == AFROS_SUCCESS) {
        kprintf("[MEM] Succès : Tâche %u compressée (Ratio 4:1).\n", task_id);
    } else {
        kprintf("[MEM] Échec : La compression a été rejetée par le matériel.\n");
    }
}
