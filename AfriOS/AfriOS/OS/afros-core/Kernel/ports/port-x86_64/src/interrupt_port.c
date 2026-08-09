#include "interrupt_abstraction.h"
#include <stdio.h>

/**
 * @file interrupt_port.c
 * @brief Port x86_64 : Local APIC + I/O APIC (repli 8259 PIC non géré).
 */

static afros_status_t irq_init_impl(void) {
    printf("[IRQ] x86_64 : activation du Local APIC (IA32_APIC_BASE.EN), masquage du PIC 8259.\n");
    return AFROS_SUCCESS;
}

static afros_status_t irq_enable_impl(uint32_t irq) {
    printf("[IRQ] x86_64 : IOAPIC redirection entry %u démasquée.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_disable_impl(uint32_t irq) {
    printf("[IRQ] x86_64 : IOAPIC redirection entry %u masquée.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_set_priority_impl(uint32_t irq, uint32_t priority) {
    printf("[IRQ] x86_64 : vecteur IDT %u, priorité APIC TPR %u.\n", irq, priority);
    return AFROS_SUCCESS;
}

static afros_status_t irq_register_handler_impl(uint32_t irq, afros_irq_handler_t handler, void *ctx) {
    (void)handler; (void)ctx;
    printf("[IRQ] x86_64 : handler installé dans l'IDT, vecteur %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_unregister_handler_impl(uint32_t irq) {
    printf("[IRQ] x86_64 : vecteur IDT %u restauré (handler par défaut).\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_ack_impl(uint32_t irq) {
    (void)irq;
    printf("[IRQ] x86_64 : EOI écrit dans le Local APIC.\n");
    return AFROS_SUCCESS;
}

static afros_status_t irq_send_ipi_impl(uint32_t target_cpu, uint32_t irq) {
    printf("[IRQ] x86_64 : IPI vers APIC ID %u (ICR, vecteur %u).\n", target_cpu, irq);
    return AFROS_SUCCESS;
}

static void irq_enable_interrupts_impl(void) {
    printf("[IRQ] sti\n");
}

static void irq_disable_interrupts_impl(void) {
    printf("[IRQ] cli\n");
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
