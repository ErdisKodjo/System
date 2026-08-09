#include "timer_abstraction.h"

/**
 * @file timer_port.c
 * @brief x86_64 Timer operations implementation using PIT
 */

#define PIT_PORT_DATA 0x40
#define PIT_PORT_COMMAND 0x43
#define PIT_FREQUENCY 1193182

static volatile uint64_t ticks = 0;
static uint32_t g_timer_frequency = 1000; // 1000 Hz default

static afros_status_t timer_init_impl(uint32_t tick_hz) {
    uint32_t divisor = PIT_FREQUENCY / tick_hz;
    g_timer_frequency = tick_hz;
    
    // Configure PIT channel 0, square wave generator
    __asm__ volatile (
        "movb $0x36, %%al\n\t"
        "outb %%al, $0x43\n\t"
        "movw %w0, %%ax\n\t"
        "outb %%al, $0x40\n\t"
        "movw %w0, %%ax\n\t"
        "shrb $8, %%ah\n\t"
        "outb %%al, $0x40\n\t"
        :
        : "r"(divisor)
        : "eax"
    );
    
    ticks = 0;
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_ticks_impl(uint64_t *tick_count) {
    if (!tick_count) return AFROS_ERROR_INVALID_PARAM;
    *tick_count = ticks;
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_frequency_hz_impl(uint32_t *freq_hz) {
    if (!freq_hz) return AFROS_ERROR_INVALID_PARAM;
    *freq_hz = g_timer_frequency;
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_oneshot_impl(uint64_t delay_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)delay_ticks;
    (void)cb;
    (void)ctx;
    // TODO: Implement one-shot timer using LAPIC or HPET
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t timer_set_periodic_impl(uint64_t period_ticks, afros_timer_callback_t cb, void *ctx) {
    (void)period_ticks;
    (void)cb;
    (void)ctx;
    // TODO: Implement periodic timer callback
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t timer_cancel_impl(void) {
    // TODO: Cancel active timer callbacks
    return AFROS_SUCCESS;
}

static afros_status_t timer_busy_wait_us_impl(uint32_t microseconds) {
    // Simple busy wait using I/O delay (port 0x80 for POST)
    for (volatile uint32_t i = 0; i < microseconds * 100; i++) {
        __asm__ volatile ("nop");
    }
    return AFROS_SUCCESS;
}

timer_ops_t arch_timer_ops = {
    .init = timer_init_impl,
    .get_ticks = timer_get_ticks_impl,
    .get_frequency_hz = timer_get_frequency_hz_impl,
    .set_oneshot = timer_set_oneshot_impl,
    .set_periodic = timer_set_periodic_impl,
    .cancel = timer_cancel_impl,
    .busy_wait_us = timer_busy_wait_us_impl
};
