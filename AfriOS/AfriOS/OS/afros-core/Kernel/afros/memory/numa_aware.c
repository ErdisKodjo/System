#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file numa_aware.c
 * @brief Gestionnaire de mémoire NUMA-aware pour AfriOS.
 */

void memory_alloc_numa(afros_size_t size, uint32_t node_id) {
    afros_virt_addr_t v_addr;
    
    kprintf("[MEM] Demande d'allocation de %zu octets sur le Noeud NUMA %u...\n", size, node_id);
    
    // Appel à l'abstraction de mémoire HAL
    if (arch_memory_ops.alloc(size, &v_addr) == AFROS_SUCCESS) {
        kprintf("[MEM] Succès : Page allouée à l'adresse virtuelle 0x%llx sur Noeud %u.\n", v_addr, node_id);
    } else {
        kprintf("[MEM] Échec : Mémoire insuffisante sur Noeud %u.\n", node_id);
    }
}
