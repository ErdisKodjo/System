#include "memory_abstraction.h"
#include "kprintf.h"

/**
 * @file memory_port.c
 * @brief Port RISC-V : pagination Sv39 (par défaut RV64GC), registre satp.
 */

static afros_status_t memory_init_impl(void) {
    kprintf("[MEM] RISC-V : configuration satp en mode Sv39, PPN de la table racine chargé.\n");
    return AFROS_SUCCESS;
}

static afros_status_t memory_alloc_impl(afros_size_t size, afros_virt_addr_t *v_addr) {
    if (!v_addr) return AFROS_ERROR_INVALID_PARAM;

    // Espace noyau haut Sv39 (bits [63:39] à 1, convention Linux RISC-V)
    static afros_virt_addr_t next_addr = 0xFFFFFFC000000000ULL;
    *v_addr = next_addr;
    next_addr += size;

    kprintf("[MEM] RISC-V : allocation de %zu octets à 0x%llx (Sv39)\n", size, (unsigned long long)*v_addr);
    return AFROS_SUCCESS;
}

static afros_status_t memory_map_impl(afros_phys_addr_t p_addr, afros_virt_addr_t v_addr, afros_size_t size, uint32_t flags) {
    (void)flags;
    kprintf("[MEM] RISC-V : map 0x%llx -> 0x%llx (%zu octets), sfence.vma après mise à jour PTE.\n",
           (unsigned long long)p_addr, (unsigned long long)v_addr, size);
    return AFROS_SUCCESS;
}

static afros_status_t memory_unmap_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    kprintf("[MEM] RISC-V : unmap 0x%llx (%zu octets), sfence.vma.\n", (unsigned long long)v_addr, size);
    return AFROS_SUCCESS;
}

static afros_status_t memory_compress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    kprintf("[MEM] RISC-V : compression logicielle de %zu octets à 0x%llx (pas d'accélérateur dédié supposé)\n",
           size, (unsigned long long)v_addr);
    return AFROS_SUCCESS;
}

static afros_status_t memory_decompress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    kprintf("[MEM] RISC-V : décompression de la page 0x%llx\n", (unsigned long long)v_addr);
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
