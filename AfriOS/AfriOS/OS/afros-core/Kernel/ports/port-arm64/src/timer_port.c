#include "timer_abstraction.h"
#include <stdio.h>

/**
 * @file timer_port.c
 * @brief Port ARM64 : ARM Generic Timer (CNTFRQ_EL0 / CNTPCT_EL0 / CNTP_TVAL_EL0).
 */

static uint32_t s_freq_hz = 62500000; // valeur type QEMU virt (CNTFRQ_EL0)
static uint64_t s_ticks = 0;

static afros_status_t timer_init_impl(uint32_t tick_hz) {
    (void)tick_hz;
    printf("[TIMER] ARM Generic Timer : lecture de CNTFRQ_EL0 = %u Hz\n", s_freq_hz);
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_ticks_impl(uint64_t *ticks) {
    if (!ticks) return AFROS_ERROR_INVALID_PARAM;
    *ticks = s_ticks++; // simulation : lirait CNTPCT_EL0
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_frequency_impl(uint32_t *freq_hz) {
    if (!freq_hz) return AFROS_ERROR_INVALID_PARAM;
    *freq_hz = s_freq_hz;
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_oneshot_impl(uint64_t delay_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)cb; (void)ctx;
    printf("[TIMER] CNTP_TVAL_EL0 = %llu (one-shot)\n", (unsigned long long)delay_ticks);
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_periodic_impl(uint64_t period_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)cb; (void)ctx;
    printf("[TIMER] CNTP_TVAL_EL0 rechargé toutes les %llu ticks (périodique)\n", (unsigned long long)period_ticks);
    return AFROS_SUCCESS;
}

static afros_status_t timer_cancel_impl(void) {
    printf("[TIMER] CNTP_CTL_EL0.ENABLE = 0\n");
    return AFROS_SUCCESS;
}

static afros_status_t timer_busy_wait_us_impl(uint32_t microseconds) {
    printf("[TIMER] Attente active %u us (boucle sur CNTPCT_EL0)\n", microseconds);
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
