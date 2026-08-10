#include "timer_abstraction.h"
#include "kprintf.h"

/**
 * @file timer_port.c
 * @brief Port MCU : SysTick (timer 24 bits intégré au cœur Cortex-M).
 */

static uint32_t s_freq_hz = 168000000; // fréquence cœur, ex. STM32F4 @168MHz
static uint64_t s_ticks = 0;

static afros_status_t timer_init_impl(uint32_t tick_hz) {
    kprintf("[TIMER] MCU : SysTick->LOAD calculé pour %u Hz (horloge cœur %u Hz).\n", tick_hz, s_freq_hz);
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_ticks_impl(uint64_t *ticks) {
    if (!ticks) return AFROS_ERROR_INVALID_PARAM;
    *ticks = s_ticks++; // simulerait un compteur logiciel incrémenté par l'ISR SysTick
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_frequency_impl(uint32_t *freq_hz) {
    if (!freq_hz) return AFROS_ERROR_INVALID_PARAM;
    *freq_hz = s_freq_hz;
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_oneshot_impl(uint64_t delay_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)cb; (void)ctx;
    kprintf("[TIMER] MCU : SysTick->VAL rechargé pour %llu ticks (one-shot, désactivé après déclenchement).\n",
           (unsigned long long)delay_ticks);
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_periodic_impl(uint64_t period_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)cb; (void)ctx;
    if (period_ticks > 0xFFFFFF) return AFROS_ERROR_INVALID_PARAM; // SysTick = 24 bits
    kprintf("[TIMER] MCU : SysTick->LOAD = %llu (périodique, mode natif du périphérique).\n",
           (unsigned long long)period_ticks);
    return AFROS_SUCCESS;
}

static afros_status_t timer_cancel_impl(void) {
    kprintf("[TIMER] MCU : SysTick->CTRL.ENABLE = 0\n");
    return AFROS_SUCCESS;
}

static afros_status_t timer_busy_wait_us_impl(uint32_t microseconds) {
    kprintf("[TIMER] MCU : attente active %u us (boucle sur SysTick->VAL)\n", microseconds);
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
