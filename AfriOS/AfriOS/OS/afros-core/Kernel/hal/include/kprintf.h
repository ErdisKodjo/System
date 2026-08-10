#ifndef AFROS_KPRINTF_H
#define AFROS_KPRINTF_H

/**
 * @file kprintf.h
 * @brief Formatted output for the freestanding AfriOS kernel.
 *
 * Drop-in replacement for libc `printf` usable before/without the C library:
 * writes through the active port's `arch_console_ops` (PL011, SBI console,
 * STM32 USART, 16550, …) selected at build time via AFROS_PORT.
 *
 * Supported conversions: %s, %d/%i, %u, %x, %X, %c, %% and the `l` length
 * modifier for %ld/%lu/%lx/%lX. No floating-point, no field width, no heap
 * allocation — suitable for early-boot and ISR-adjacent logging.
 *
 * When AFROS_FREESTANDING is NOT defined (host simulator build), kprintf
 * delegates to libc vprintf so the existing afros-kernel-sim still produces
 * visible output without real hardware.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format a string and emit it via arch_console_ops.
 * @returns number of characters emitted (best-effort, like printf).
 */
int kprintf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* AFROS_KPRINTF_H */
