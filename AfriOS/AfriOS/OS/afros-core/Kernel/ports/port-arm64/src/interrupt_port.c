#include "interrupt_abstraction.h"
#include <stdio.h>

/**
 * @file interrupt_port.c
 * @brief Port ARM64 : contrôleur d'interruptions GICv3 (Distributor + Redistributor).
 */

#define GICV3_MAX_IRQ 1020

static afros_status_t irq_init_impl(void) {
    printf("[IRQ] GICv3 : initialisation du Distributor et des Redistributors...\n");
    return AFROS_SUCCESS;
}

static afros_status_t irq_enable_impl(uint32_t irq) {
    if (irq >= GICV3_MAX_IRQ) return AFROS_ERROR_INVALID_PARAM;
    printf("[IRQ] GICv3 : IRQ %u activée (ISENABLER).\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_disable_impl(uint32_t irq) {
    if (irq >= GICV3_MAX_IRQ) return AFROS_ERROR_INVALID_PARAM;
    printf("[IRQ] GICv3 : IRQ %u désactivée (ICENABLER).\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_set_priority_impl(uint32_t irq, uint32_t priority) {
    printf("[IRQ] GICv3 : IRQ %u priorité %u (IPRIORITYR).\n", irq, priority);
    return AFROS_SUCCESS;
}

static afros_status_t irq_register_handler_impl(uint32_t irq, afros_irq_handler_t handler, void *ctx) {
    (void)handler; (void)ctx;
    printf("[IRQ] GICv3 : handler enregistré pour IRQ %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_unregister_handler_impl(uint32_t irq) {
    printf("[IRQ] GICv3 : handler retiré pour IRQ %u.\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_ack_impl(uint32_t irq) {
    printf("[IRQ] GICv3 : acquittement IRQ %u (EOIR1_EL1).\n", irq);
    return AFROS_SUCCESS;
}

static afros_status_t irq_send_ipi_impl(uint32_t target_cpu, uint32_t irq) {
    printf("[IRQ] GICv3 : IPI vers CPU %u (SGI %u via ICC_SGI1R_EL1).\n", target_cpu, irq);
    return AFROS_SUCCESS;
}

static void irq_enable_interrupts_impl(void) {
    printf("[IRQ] msr daifclr, #0xf\n");
}

static void irq_disable_interrupts_impl(void) {
    printf("[IRQ] msr daifset, #0xf\n");
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
