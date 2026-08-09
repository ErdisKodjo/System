#include "memory_abstraction.h"

/**
 * @file memory_port.c
 * @brief x86_64 Memory operations implementation
 */

#define PAGE_SIZE 4096
#define KERNEL_HEAP_START 0x00200000
#define KERNEL_HEAP_SIZE (128 * 1024 * 1024)

static uint8_t heap_bitmap[PAGE_SIZE];
static size_t heap_start = KERNEL_HEAP_START;

static afros_status_t memory_init_impl(void) {
    for (size_t i = 0; i < sizeof(heap_bitmap); i++) {
        heap_bitmap[i] = 0;
    }
    return AFROS_SUCCESS;
}

static afros_status_t memory_alloc_impl(afros_size_t size, afros_virt_addr_t *v_addr) {
    if (!v_addr || size == 0) return AFROS_ERROR_INVALID_PARAM;
    
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (size_t i = 0; i < sizeof(heap_bitmap) * 8; i++) {
        if (!(heap_bitmap[i / 8] & (1 << (i % 8)))) {
            size_t free_pages = 0;
            for (size_t j = i; j < sizeof(heap_bitmap) * 8 && j < i + pages; j++) {
                if (heap_bitmap[j / 8] & (1 << (j % 8))) break;
                free_pages++;
            }
            
            if (free_pages >= pages) {
                for (size_t j = i; j < i + pages; j++) {
                    heap_bitmap[j / 8] |= (1 << (j % 8));
                }
                
                *v_addr = heap_start + (i * PAGE_SIZE);
                return AFROS_SUCCESS;
            }
        }
    }
    
    return AFROS_ERROR_NO_MEMORY;
}

static afros_status_t memory_free_impl(afros_virt_addr_t v_addr) {
    if (v_addr < heap_start) return AFROS_ERROR_INVALID_PARAM;
    
    size_t offset = (v_addr - heap_start) / PAGE_SIZE;
    
    if (offset >= sizeof(heap_bitmap) * 8) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    
    /* Clear one page for simplicity */
    heap_bitmap[offset / 8] &= ~(1 << (offset % 8));
    
    return AFROS_SUCCESS;
}

static afros_status_t memory_map_impl(afros_phys_addr_t p_addr, afros_virt_addr_t v_addr, afros_size_t size, uint32_t flags) {
    (void)p_addr;
    (void)v_addr;
    (void)size;
    (void)flags;
    /* Identity mapping assumed for bare-metal */
    return AFROS_SUCCESS;
}

static afros_status_t memory_unmap_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr;
    (void)size;
    return AFROS_SUCCESS;
}

static afros_status_t memory_compress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr;
    (void)size;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t memory_decompress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr;
    (void)size;
    return AFROS_ERROR_NOT_SUPPORTED;
}

memory_ops_t arch_memory_ops = {
    .init = memory_init_impl,
    .alloc = memory_alloc_impl,
    .free = memory_free_impl,
    .map = memory_map_impl,
    .unmap = memory_unmap_impl,
    .compress = memory_compress_impl,
    .decompress = memory_decompress_impl
};
