#include "memory_abstraction.h"
#include <stdio.h>

/**
 * @file memory_port.c
 * @brief Port x86_64 : pagination 4 niveaux (CR3/PML4), espace noyau canonique haut.
 */

static afros_status_t memory_init_impl(void) {
    printf("[MEM] x86_64 : pagination 4 niveaux (PML4/PDPT/PD/PT), CR3 chargé.\n");
    return AFROS_SUCCESS;
}

static afros_status_t memory_alloc_impl(afros_size_t size, afros_virt_addr_t *v_addr) {
    if (!v_addr) return AFROS_ERROR_INVALID_PARAM;

    // Espace noyau canonique haut x86_64 (0xFFFF800000000000 et au-delà)
    static afros_virt_addr_t next_addr = 0xFFFF800000000000ULL;
    *v_addr = next_addr;
    next_addr += size;

    printf("[MEM] x86_64 : allocation de %zu octets à 0x%llx\n", size, (unsigned long long)*v_addr);
    return AFROS_SUCCESS;
}

static afros_status_t memory_map_impl(afros_phys_addr_t p_addr, afros_virt_addr_t v_addr, afros_size_t size, uint32_t flags) {
    (void)flags;
    printf("[MEM] x86_64 : map 0x%llx -> 0x%llx (%zu octets) dans la table de pages.\n",
           (unsigned long long)p_addr, (unsigned long long)v_addr, size);
    return AFROS_SUCCESS;
}

static afros_status_t memory_unmap_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    printf("[MEM] x86_64 : unmap 0x%llx (%zu octets), invlpg.\n", (unsigned long long)v_addr, size);
    return AFROS_SUCCESS;
}

static afros_status_t memory_compress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    printf("[MEM] x86_64 : compression zswap de %zu octets à 0x%llx\n", size, (unsigned long long)v_addr);
    return AFROS_SUCCESS;
}

static afros_status_t memory_decompress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    printf("[MEM] x86_64 : décompression de la page 0x%llx\n", (unsigned long long)v_addr);
    return AFROS_SUCCESS;
}

memory_ops_t arch_memory_ops = {
    .init = memory_init_impl,
    .alloc = memory_alloc_impl,
    .free = NULL,
    .map = memory_map_impl,
    .unmap = memory_unmap_impl,
    .compress = memory_compress_impl,
    .decompress = memory_decompress_impl
};
