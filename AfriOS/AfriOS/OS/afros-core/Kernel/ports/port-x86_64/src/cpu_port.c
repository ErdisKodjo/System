#include "cpu_abstraction.h"

/**
 * @file cpu_port.c
 * @brief x86_64 CPU operations implementation
 */

static afros_status_t cpu_init_impl(void) {
    __asm__ volatile (
        "cli\n\t"
        "lgdt idt_descriptor\n\t"
        "sti\n\t"
    );
    return AFROS_SUCCESS;
}

static afros_status_t cpu_get_info_impl(uint32_t cpu_id, afros_cpu_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    
    info->core_id = cpu_id;
    info->cluster_id = 0;
    info->is_big = true;
    info->current_freq_mhz = 2400;
    
    return AFROS_SUCCESS;
}

static afros_status_t cpu_set_frequency_impl(uint32_t cpu_id, uint32_t freq_mhz) {
    /* Frequency scaling not yet implemented */
    (void)cpu_id;
    (void)freq_mhz;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t cpu_sleep_core_impl(uint32_t cpu_id) {
    (void)cpu_id;
    __asm__ volatile ("hlt");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_wakeup_core_impl(uint32_t cpu_id) {
    (void)cpu_id;
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t cpu_migrate_task_impl(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id) {
    (void)from_cpu;
    (void)to_cpu;
    (void)task_id;
    return AFROS_ERROR_NOT_SUPPORTED;
}

cpu_ops_t arch_cpu_ops = {
    .init = cpu_init_impl,
    .get_info = cpu_get_info_impl,
    .set_frequency = cpu_set_frequency_impl,
    .sleep_core = cpu_sleep_core_impl,
    .wakeup_core = cpu_wakeup_core_impl,
    .migrate_task = cpu_migrate_task_impl
};
