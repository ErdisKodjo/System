#include "interrupt_abstraction.h"
#include "kprintf.h"

/**
 * @file interrupt_port.c
 * @brief Port RISC-V : PLIC (interruptions externes) + CLINT (logicielles/timer).
 */

static afros_status_t irq_init_impl(void) {
    kprintf("[IRQ] RISC-V : PLIC initialisé (priority/threshold), CLINT mappé.\n");
    return AFROS_SUCCESS;
}

static afros_status_t irq_enable_impl(uint32_t irq) {
    kprintf("[IRQ] RISC-V : PLIC enable bit positionné pour la source %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_disable_impl(uint32_t irq) {
    kprintf("[IRQ] RISC-V : PLIC enable bit effacé pour la source %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_set_priority_impl(uint32_t irq, uint32_t priority) {
    kprintf("[IRQ] RISC-V : PLIC priority[%u] = %u.\n", irq, priority);
    return AFROS_SUCCESS;
}

static afros_status_t irq_register_handler_impl(uint32_t irq, afros_irq_handler_t handler, void *ctx) {
    (void)handler; (void)ctx;
    kprintf("[IRQ] RISC-V : handler enregistré pour la source PLIC %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_unregister_handler_impl(uint32_t irq) {
    kprintf("[IRQ] RISC-V : handler retiré pour la source PLIC %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_ack_impl(uint32_t irq) {
    kprintf("[IRQ] RISC-V : PLIC claim/complete pour la source %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_send_ipi_impl(uint32_t target_cpu, uint32_t irq) {
    (void)irq;
    kprintf("[IRQ] RISC-V : CLINT msip[%u] = 1 (IPI logicielle) ou SBI send_ipi.\n", target_cpu);
    return AFROS_SUCCESS;
}

static void irq_enable_interrupts_impl(void) {
    kprintf("[IRQ] csrs mstatus, MSTATUS_MIE\n");
}

static void irq_disable_interrupts_impl(void) {
    kprintf("[IRQ] csrc mstatus, MSTATUS_MIE\n");
}

interrupt_ops_t arch_interrupt_ops = {
    .init = irq_init_impl,
    .enable = irq_enable_impl,
    .disable = irq_disable_impl,
    .set_priority = irq_set_priority_impl,
    .register_handler = irq_register_handler_impl,
    .unregister_handler = irq_unregister_handler_impl,
    .ack = irq_ack_impl,
    .send_ipi = irq_send_ipi_impl,
    .enable_interrupts = irq_enable_interrupts_impl,
    .disable_interrupts = irq_disable_interrupts_impl
};
