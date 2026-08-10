#include "interrupt_abstraction.h"

/**
 * @file interrupt_port.c
 * @brief x86_64 Interrupt operations implementation
 */

static afros_status_t interrupt_init_impl(void) {
    __asm__ volatile (
        "cli\n\t"
        "lidt idt_descriptor\n\t"
        "sti\n\t"
    );
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_enable_impl(uint32_t irq) {
    (void)irq;
    __asm__ volatile ("sti");
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_disable_impl(uint32_t irq) {
    (void)irq;
    __asm__ volatile ("cli");
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_set_priority_impl(uint32_t irq, uint32_t priority) {
    (void)irq;
    (void)priority;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t interrupt_register_handler_impl(uint32_t irq, afros_irq_handler_t handler, void *ctx) {
    (void)irq;
    (void)handler;
    (void)ctx;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t interrupt_unregister_handler_impl(uint32_t irq) {
    (void)irq;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t interrupt_ack_impl(uint32_t irq) {
    (void)irq;
    // Send EOI (End Of Interrupt) to PIC
    __asm__ volatile (
        "movb $0x20, %%al\n\t"
        "outb %%al, $0x20\n\t"
        :
        :
        : "al"
    );
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_send_ipi_impl(uint32_t target_cpu, uint32_t irq) {
    (void)target_cpu;
    (void)irq;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static void interrupt_enable_interrupts_impl(void) {
    __asm__ volatile ("sti");
}

static void interrupt_disable_interrupts_impl(void) {
    __asm__ volatile ("cli");
}

interrupt_ops_t arch_interrupt_ops = {
    .init = interrupt_init_impl,
    .enable = interrupt_enable_impl,
    .disable = interrupt_disable_impl,
    .set_priority = interrupt_set_priority_impl,
    .register_handler = interrupt_register_handler_impl,
    .unregister_handler = interrupt_unregister_handler_impl,
    .ack = interrupt_ack_impl,
    .send_ipi = interrupt_send_ipi_impl,
    .enable_interrupts = interrupt_enable_interrupts_impl,
    .disable_interrupts = interrupt_disable_interrupts_impl
};
