#ifndef TIMER_ABSTRACTION_H
#define TIMER_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file timer_abstraction.h
 * @brief System timer abstraction for AfriOS (ARM Generic Timer / TSC-APIC-timer /
 *        RISC-V CLINT mtime / SysTick).
 */

typedef void (*afros_timer_callback_t)(void *ctx);

typedef struct {
    afros_status_t (*init)(uint32_t tick_hz);
    afros_status_t (*get_ticks)(uint64_t *ticks);
    afros_status_t (*get_frequency_hz)(uint32_t *freq_hz);
    afros_status_t (*set_oneshot)(uint64_t delay_ticks, afros_timer_callback_t cb, void *ctx);
    afros_status_t (*set_periodic)(uint64_t period_ticks, afros_timer_callback_t cb, void *ctx);
    afros_status_t (*cancel)(void);
    afros_status_t (*busy_wait_us)(uint32_t microseconds);
} timer_ops_t;

extern timer_ops_t arch_timer_ops;

#endif // TIMER_ABSTRACTION_H
