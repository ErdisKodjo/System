#include "interrupt_abstraction.h"
#include "kprintf.h"

/**
 * @file interrupt_port.c
 * @brief Port MCU : NVIC (Nested Vectored Interrupt Controller), priorités sur
 *        3-4 bits selon le cœur. Pas d'IPI : un seul cœur, send_ipi non supporté.
 */

static afros_status_t irq_init_impl(void) {
    kprintf("[IRQ] MCU : NVIC initialisé, table de vecteurs à VTOR.\n");
    return AFROS_SUCCESS;
}

static afros_status_t irq_enable_impl(uint32_t irq) {
    kprintf("[IRQ] MCU : NVIC->ISER, IRQ %u activée.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_disable_impl(uint32_t irq) {
    kprintf("[IRQ] MCU : NVIC->ICER, IRQ %u désactivée.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_set_priority_impl(uint32_t irq, uint32_t priority) {
    if (priority > 15) return AFROS_ERROR_INVALID_PARAM; // ex. 4 bits de priorité
    kprintf("[IRQ] MCU : NVIC->IPR[%u] = %u.\n", irq, priority);
    return AFROS_SUCCESS;
}

static afros_status_t irq_register_handler_impl(uint32_t irq, afros_irq_handler_t handler, void *ctx) {
    (void)handler; (void)ctx;
    kprintf("[IRQ] MCU : handler installé dans la table de vecteurs, IRQ %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_unregister_handler_impl(uint32_t irq) {
    kprintf("[IRQ] MCU : handler par défaut restauré, IRQ %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_ack_impl(uint32_t irq) {
    (void)irq;
    // Sur Cortex-M, l'acquittement est automatique en sortie d'ISR (pas
    // d'écriture explicite comme EOI ARM64/x86) : no-op documenté.
    return AFROS_SUCCESS;
}

static afros_status_t irq_send_ipi_impl(uint32_t target_cpu, uint32_t irq) {
    (void)target_cpu; (void)irq;
    return AFROS_ERROR_NOT_SUPPORTED; // mono-cœur : pas d'IPI
}

static void irq_enable_interrupts_impl(void) {
    kprintf("[IRQ] cpsie i\n");
}

static void irq_disable_interrupts_impl(void) {
    kprintf("[IRQ] cpsid i\n");
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
