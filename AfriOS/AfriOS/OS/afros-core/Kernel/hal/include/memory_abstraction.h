#ifndef MEMORY_ABSTRACTION_H
#define MEMORY_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file memory_abstraction.h
 * @brief Memory abstraction layer for AfriOS.
 */

typedef struct {
    afros_status_t (*init)(void);
    afros_status_t (*alloc)(afros_size_t size, afros_virt_addr_t *v_addr);
    afros_status_t (*free)(afros_virt_addr_t v_addr);
    afros_status_t (*map)(afros_phys_addr_t p_addr, afros_virt_addr_t v_addr, afros_size_t size, uint32_t flags);
    afros_status_t (*unmap)(afros_virt_addr_t v_addr, afros_size_t size);
    afros_status_t (*compress)(afros_virt_addr_t v_addr, afros_size_t size);
    afros_status_t (*decompress)(afros_virt_addr_t v_addr, afros_size_t size);
} memory_ops_t;

extern memory_ops_t arch_memory_ops;

#endif // MEMORY_ABSTRACTION_H
