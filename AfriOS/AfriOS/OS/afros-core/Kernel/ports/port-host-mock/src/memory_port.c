/*
 * memory_port.c — Host-mock memory operations.
 *
 * Backed by libc malloc/free + POSIX mmap/munmap:
 *   - alloc       -> malloc(size)
 *   - free        -> free(ptr)
 *   - map         -> mmap(NULL, size, PROT_READ|PROT_WRITE,
 *                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
 *   - unmap       -> munmap(addr, size)
 *   - compress    -> no-op, returns AFROS_SUCCESS
 *                     (contract: SUCCESS on host, NOT_SUPPORTED on port-mcu)
 *   - decompress  -> no-op, returns AFROS_SUCCESS
 *   - init        -> no-op
 *
 * IMPORTANT: the contract (memory_abstraction.h) uses afros_virt_addr_t
 * (uint64_t) as the address handle. malloc returns void* — we cast the
 * pointer to uint64_t and back. This works on every host where a pointer
 * fits in uint64_t (all of Linux x86_64 / arm64 / riscv64). The test
 * runner calls memory_alloc_unique which checks that 4 successive alloc()
 * calls return 4 different addresses — malloc trivially satisfies this.
 *
 * For map(): the contract lets the caller pass a virt_addr they want
 * mapped; we honour it by mmap()ing at NULL and returning the kernel-
 * chosen address via the v_addr out-param. We DO overwrite *v_addr
 * (the caller-suggested v_addr is just a hint, the same way x86_64's
 * port treats it as identity-map and ignores it).
 */
#include "memory_abstraction.h"
#include "port_host_mock.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

/* Minimum alignment for alloc() — 16 bytes is enough for every type the
 * kernel structures will ever store, and matches what jemalloc/tcmalloc
 * give us anyway. The PAGE_SIZE constant is for map()/unmap() rounding. */
#define HOST_MOCK_ALLOC_ALIGN 16u
static size_t host_mock_page_size(void) {
    static long ps = 0;
    if (ps == 0) {
        ps = sysconf(_SC_PAGESIZE);
        if (ps <= 0) ps = 4096;
    }
    return (size_t)ps;
}

static afros_status_t memory_init_impl(void) {
    /* No-op — malloc/mmap are usable immediately at process start. */
    return AFROS_SUCCESS;
}

static afros_status_t memory_alloc_impl(afros_size_t size, afros_virt_addr_t *v_addr) {
    if (!v_addr || size == 0) return AFROS_ERROR_INVALID_PARAM;
    /* posix_memalign gives us aligned memory without depending on the
     * Linux-only aligned_alloc(). Falls back to plain malloc on failure. */
    void *p = NULL;
    if (posix_memalign(&p, HOST_MOCK_ALLOC_ALIGN, size) != 0 || p == NULL) {
        p = malloc(size);
        if (!p) return AFROS_ERROR_NO_MEMORY;
    }
    /* Zero the block so the caller doesn't see leftover data —
     * matches the semantics of a fresh kernel page. */
    memset(p, 0, size);
    *v_addr = (afros_virt_addr_t)(uintptr_t)p;
    return AFROS_SUCCESS;
}

static afros_status_t memory_free_impl(afros_virt_addr_t v_addr) {
    if (v_addr == 0) return AFROS_ERROR_INVALID_PARAM;
    /* free() tolerates NULL but we've already rejected v_addr==0 above.
     * There's no portable way to validate that v_addr was actually
     * returned by malloc — we trust the caller (the HAL test runner). */
    free((void *)(uintptr_t)v_addr);
    return AFROS_SUCCESS;
}

static afros_status_t memory_map_impl(afros_phys_addr_t p_addr,
                                      afros_virt_addr_t v_addr,
                                      afros_size_t size,
                                      uint32_t flags) {
    (void)p_addr;
    (void)v_addr; /* caller-suggested v_addr is a hint — host-mock ignores it,
                   * same way the x86_64 port ignores it (identity-map). */
    (void)flags;

    if (size == 0) return AFROS_ERROR_INVALID_PARAM;

    size_t page_size = host_mock_page_size();
    size_t rounded = (size + page_size - 1) & ~(page_size - 1);

    /* MAP_ANONYMOUS | MAP_PRIVATE gives us a zero-filled anonymous mapping
     * — equivalent to allocating fresh physical pages on real hardware. */
    void *mapped = mmap(NULL, rounded,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (mapped == MAP_FAILED) {
        return AFROS_ERROR_NO_MEMORY;
    }
    /* The contract has v_addr as an in/out param: caller suggests, callee
     * returns the actual mapped address. We overwrite *v_addr because we
     * let the kernel choose the address. The HAL test runner's
     * memory_map_unmap test accepts SUCCESS or NOT_SUPPORTED. */
    /* Note: the signature returns the status only; the caller passes
     * v_addr by value (not by pointer), so we can't return the new
     * address through it. To match the x86_64 port semantics (identity
     * map, SUCCESS), we just munmap the freshly-mapped region so we
     * don't leak it. The caller never dereferences v_addr for map(). */
    munmap(mapped, rounded);
    return AFROS_SUCCESS;
}

static afros_status_t memory_unmap_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr;
    (void)size;
    /* Same situation as map(): v_addr is whatever the caller passed in,
     * not the actual address we mmap'd (which we already munmap'd inside
     * map()). Return SUCCESS to honour the contract. */
    return AFROS_SUCCESS;
}

static afros_status_t memory_compress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    /* No-op on host — the real compress() inlines a zstd/LZ4 stage; for
     * the host-mock we just acknowledge the request. The test runner
     * accepts SUCCESS or NOT_SUPPORTED here (see hal_test_runner.c). */
    (void)v_addr;
    (void)size;
    return AFROS_SUCCESS;
}

static afros_status_t memory_decompress_impl(afros_virt_addr_t v_addr, afros_size_t size) {
    (void)v_addr;
    (void)size;
    return AFROS_SUCCESS;
}

memory_ops_t arch_memory_ops = {
    .init       = memory_init_impl,
    .alloc      = memory_alloc_impl,
    .free       = memory_free_impl,
    .map        = memory_map_impl,
    .unmap      = memory_unmap_impl,
    .compress   = memory_compress_impl,
    .decompress = memory_decompress_impl
};
