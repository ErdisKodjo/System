/*
 * interrupt_port.c — Host-mock interrupt operations.
 *
 * Userspace-safe no-ops for every IRQ primitive — there is no PIC/GIC/PLIC
 * in a Linux userspace process. The contract (interrupt_abstraction.h) is
 * preserved so the HAL test runner can validate the API surface.
 *
 *   - init               -> no-op
 *   - enable/disable     -> no-op returns SUCCESS
 *   - set_priority       -> no-op returns NOT_SUPPORTED (matches x86_64 port)
 *   - register_handler   -> no-op returns SUCCESS (host-mock has no
 *                           real IRQ to dispatch, but accepts the registration)
 *   - unregister_handler -> no-op returns SUCCESS
 *   - ack                -> no-op (no EOI to send on host)
 *   - send_ipi           -> returns NOT_SUPPORTED (no SMP, single-process)
 *   - enable_interrupts  -> no-op (no `sti`)
 *   - disable_interrupts -> no-op (no `cli`)
 */
#include "interrupt_abstraction.h"
#include "port_host_mock.h"

/* Storage for the (single) registered handler so that register/unregister
 * have observable state — without this the test runner's contract checks
 * would be vacuous. */
static afros_irq_handler_t s_registered_handler = NULL;
static void               *s_registered_ctx      = NULL;
static uint32_t            s_registered_irq      = 0xFFFFFFFFu;

static afros_status_t interrupt_init_impl(void) {
    s_registered_handler = NULL;
    s_registered_ctx     = NULL;
    s_registered_irq     = 0xFFFFFFFFu;
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_enable_impl(uint32_t irq) {
    (void)irq;
    /* No PIC/GIC to mask/unmask on host. */
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_disable_impl(uint32_t irq) {
    (void)irq;
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_set_priority_impl(uint32_t irq, uint32_t priority) {
    (void)irq;
    (void)priority;
    /* Match the x86_64 port contract — set_priority is NOT_SUPPORTED. */
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t interrupt_register_handler_impl(uint32_t irq,
                                                     afros_irq_handler_t handler,
                                                     void *ctx) {
    /* Accept the registration. On host we'll never actually fire the
     * handler (there's no real IRQ source), but we record it so a
     * subsequent unregister_handler(irq) can return SUCCESS. */
    s_registered_irq     = irq;
    s_registered_handler = handler;
    s_registered_ctx     = ctx;
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_unregister_handler_impl(uint32_t irq) {
    if (irq != s_registered_irq) {
        /* Nothing was registered for this irq — mirror the x86_64 port
         * which would return NOT_SUPPORTED for an unregistered irq.
         * The HAL test runner doesn't exercise this case directly. */
        return AFROS_ERROR_NOT_FOUND;
    }
    s_registered_handler = NULL;
    s_registered_ctx     = NULL;
    s_registered_irq     = 0xFFFFFFFFu;
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_ack_impl(uint32_t irq) {
    (void)irq;
    /* No EOI to send on host — no PIC to acknowledge. */
    return AFROS_SUCCESS;
}

static afros_status_t interrupt_send_ipi_impl(uint32_t target_cpu, uint32_t irq) {
    (void)target_cpu;
    (void)irq;
    /* No SMP on host-mock (single-process). The test runner explicitly
     * expects NOT_SUPPORTED here (see hal_test_runner.c
     * test_interrupt_send_ipi_port_mcu_unsupported). */
    return AFROS_ERROR_NOT_SUPPORTED;
}

static void interrupt_enable_interrupts_impl(void) {
    /* No `sti` on host — would be a no-op even if we tried. */
}

static void interrupt_disable_interrupts_impl(void) {
    /* No `cli` on host. */
}

interrupt_ops_t arch_interrupt_ops = {
    .init              = interrupt_init_impl,
    .enable            = interrupt_enable_impl,
    .disable           = interrupt_disable_impl,
    .set_priority      = interrupt_set_priority_impl,
    .register_handler  = interrupt_register_handler_impl,
    .unregister_handler= interrupt_unregister_handler_impl,
    .ack               = interrupt_ack_impl,
    .send_ipi          = interrupt_send_ipi_impl,
    .enable_interrupts = interrupt_enable_interrupts_impl,
    .disable_interrupts= interrupt_disable_interrupts_impl
};
