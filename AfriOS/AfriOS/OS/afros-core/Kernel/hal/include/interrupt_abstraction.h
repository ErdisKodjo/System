#ifndef INTERRUPT_ABSTRACTION_H
#define INTERRUPT_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file interrupt_abstraction.h
 * @brief Interrupt controller abstraction for AfriOS (GICv3 / APIC / PLIC+CLINT / NVIC).
 */

typedef void (*afros_irq_handler_t)(uint32_t irq, void *ctx);

typedef struct {
    afros_status_t (*init)(void);
    afros_status_t (*enable)(uint32_t irq);
    afros_status_t (*disable)(uint32_t irq);
    afros_status_t (*set_priority)(uint32_t irq, uint32_t priority);
    afros_status_t (*register_handler)(uint32_t irq, afros_irq_handler_t handler, void *ctx);
    afros_status_t (*unregister_handler)(uint32_t irq);
    afros_status_t (*ack)(uint32_t irq);
    afros_status_t (*send_ipi)(uint32_t target_cpu, uint32_t irq);
    void (*enable_interrupts)(void);
    void (*disable_interrupts)(void);
} interrupt_ops_t;

extern interrupt_ops_t arch_interrupt_ops;

#endif // INTERRUPT_ABSTRACTION_H
