#ifndef AFROS_CPU_IDLE_H
#define AFROS_CPU_IDLE_H

/**
 * @file afros_cpu_idle.h
 * @brief Multi-architecture CPU idle/halt primitive.
 *
 * The HAL sources (hal_init.c, power_manager.c) need to put the CPU in a
 * low-power idle state when waiting for an interrupt or halting the system.
 * The instruction to do this is architecture-specific:
 *
 *   - x86_64:        `hlt` (halt until next interrupt)
 *   - ARM64 (AArch64): `wfi` (Wait For Interrupt)
 *   - RISC-V:        `wfi` (Wait For Interrupt)
 *   - MCU Cortex-M:  `wfi` (Wait For Interrupt)
 *   - host-mock:     no-op (userspace has no halt instruction)
 *
 * This header provides a single `afros_cpu_halt()` macro that expands to the
 * correct instruction per architecture. It is safe to include from any HAL
 * source file.
 */

#include "afros_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Put the CPU in a low-power idle state until the next interrupt.
 *
 * On x86_64:  emits `hlt`
 * On ARM64:   emits `wfi`
 * On RISC-V:  emits `wfi`
 * On MCU:     emits `wfi`
 * On host:    no-op (just a busy loop)
 *
 * This function never returns normally — it only resumes when an interrupt
 * fires. For permanent halt (system shutdown), call in a `while(1)` loop.
 */
static inline void afros_cpu_halt(void) {
#if defined(__x86_64__)
    __asm__ volatile ("hlt");
#elif defined(__aarch64__)
    __asm__ volatile ("wfi");
#elif defined(__riscv)
    __asm__ volatile ("wfi");
#elif defined(__thumb__) || defined(__arm__)
    /* Cortex-M uses WFI in Thumb mode */
    __asm__ volatile ("wfi");
#else
    /* host-mock or unknown arch: no-op (userspace can't halt) */
    /* Volatile to prevent compiler from optimizing the loop away */
    __asm__ volatile ("" ::: "memory");
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* AFROS_CPU_IDLE_H */
