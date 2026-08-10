#include "memory_abstraction.h"
#include "kprintf.h"

/**
 * @file memory_port.c
 * @brief Port MCU : pas de MMU — modèle mémoire plat, allocation statique dans
 *        la SRAM interne. map/unmap et compress/decompress ne s'appliquent pas
 *        (pas de pagination, pas de budget CPU pour de la compression mémoire).
 */

#define MCU_SRAM_BASE 0x20000000UL // convention Cortex-M (SRAM)
#define MCU_SRAM_SIZE (128 * 1024) // exemple : 128 Ko de SRAM interne

static afros_size_t s_allocated = 0;

static afros_status_t memory_init_impl(void) {
    kprintf("[MEM] MCU : SRAM interne à 0x%lx, %u octets, pas de MMU (modèle plat).\n",
           MCU_SRAM_BASE, MCU_SRAM_SIZE);
    return AFROS_SUCCESS;
}

static afros_status_t memory_alloc_impl(afros_size_t size, afros_virt_addr_t *v_addr) {
    if (!v_addr) return AFROS_ERROR_INVALID_PARAM;
    if (s_allocated + size > MCU_SRAM_SIZE) return AFROS_ERROR_NO_MEMORY;

    // Adresse "virtuelle" == adresse physique : pas de traduction sur ce port.
    *v_addr = MCU_SRAM_BASE + s_allocated;
    s_allocated += size;

    kprintf("[MEM] MCU : allocation statique de %zu octets à 0x%llx\n", size, (unsigned long long)*v_addr);
    return AFROS_SUCCESS;
}

static afros_status_t memory_map_impl(afros_phys_addr_t p_addr, afros_virt_addr_t v_addr, afros_size_t size, uint32_t flags) {
    (void)p_addr; (void)v_addr; (void)size; (void)flags;
    kprintf("[MEM] MCU : pas de MMU, map() non applicable.\n");
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t memory_unmap_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr; (void)size;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t memory_compress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr; (void)size;
    // Pas de ZRAM sur MCU : la SRAM est trop petite pour justifier le coût CPU
    // de la (dé)compression, et il n'y a pas de swap.
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t memory_decompress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr; (void)size;
    return AFROS_ERROR_NOT_SUPPORTED;
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
