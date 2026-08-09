#include "timer_abstraction.h"
#include <stdio.h>

/**
 * @file timer_port.c
 * @brief Port RISC-V : CLINT mtime/mtimecmp, ou extension SBI TIME en mode superviseur.
 */

static uint32_t s_freq_hz = 10000000; // fréquence type QEMU virt (10 MHz)
static uint64_t s_ticks = 0;

static afros_status_t timer_init_impl(uint32_t tick_hz) {
    (void)tick_hz;
    printf("[TIMER] RISC-V : CLINT mtime, fréquence = %u Hz (timebase-frequency du DT).\n", s_freq_hz);
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_ticks_impl(uint64_t *ticks) {
    if (!ticks) return AFROS_ERROR_INVALID_PARAM;
    *ticks = s_ticks++; // simulerait la lecture de mtime (ou csr rdtime en mode S)
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_frequency_impl(uint32_t *freq_hz) {
    if (!freq_hz) return AFROS_ERROR_INVALID_PARAM;
    *freq_hz = s_freq_hz;
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_oneshot_impl(uint64_t delay_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)cb; (void)ctx;
    printf("[TIMER] RISC-V : mtimecmp = mtime + %llu, ou SBI sbi_set_timer() (one-shot)\n", (unsigned long long)delay_ticks);
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_periodic_impl(uint64_t period_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)cb; (void)ctx;
    printf("[TIMER] RISC-V : ré-armement mtimecmp toutes les %llu ticks (périodique, ISR déclenche le suivant)\n",
           (unsigned long long)period_ticks);
    return AFROS_SUCCESS;
}

static afros_status_t timer_cancel_impl(void) {
    printf("[TIMER] RISC-V : mtimecmp = UINT64_MAX (désarmé)\n");
    return AFROS_SUCCESS;
}

static afros_status_t timer_busy_wait_us_impl(uint32_t microseconds) {
    printf("[TIMER] RISC-V : attente active %u us (boucle sur mtime)\n", microseconds);
    return AFROS_SUCCESS;
}

timer_ops_t arch_timer_ops = {
    .init = timer_init_impl,
    .get_ticks = timer_get_ticks_impl,
    .get_frequency_hz = timer_get_frequency_impl,
    .set_oneshot = timer_set_oneshot_impl,
    .set_periodic = timer_set_periodic_impl,
    .cancel = timer_cancel_impl,
    .busy_wait_us = timer_busy_wait_us_impl
};
