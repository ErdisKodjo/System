#include "cpu_abstraction.h"
#include <stdio.h>

/**
 * @file cpu_port.c
 * @brief Port x86_64 : topologie hybride P-core/E-core (Intel) ou CCX (AMD),
 *        détection via CPUID, fréquence via MSR IA32_PERF_CTL.
 */

static afros_status_t cpu_init_impl(void) {
    printf("[CPU] x86_64 : CPUID.01H/0BH pour la topologie, détection P-core/E-core...\n");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_get_info_impl(uint32_t cpu_id, afros_cpu_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;

    // Simulation 8 threads logiques : 4 P-core (0-3, avec HT), 4 E-core (4-7, sans HT)
    info->core_id = cpu_id;
    info->cluster_id = (cpu_id < 4) ? 0 : 1;
    info->is_big = (cpu_id < 4); // "big" = P-core ici (inverse d'ARM où big est en fin de plage)
    info->current_freq_mhz = (info->is_big) ? 3600 : 2400;

    return AFROS_SUCCESS;
}

static afros_status_t cpu_set_frequency_impl(uint32_t cpu_id, uint32_t freq_mhz) {
    printf("[CPU] x86_64 : Core %u -> IA32_PERF_CTL = %u MHz (P-state).\n", cpu_id, freq_mhz);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_sleep_core_impl(uint32_t cpu_id) {
    printf("[CPU] x86_64 : Core %u -> MWAIT (C-state).\n", cpu_id);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_wakeup_core_impl(uint32_t cpu_id) {
    printf("[CPU] x86_64 : Core %u -> IPI de réveil (APIC).\n", cpu_id);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_migrate_task_impl(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id) {
    printf("[CPU] x86_64 : Tâche %u migrée du Core %u vers le Core %u (affinity mask).\n", task_id, from_cpu, to_cpu);
    return AFROS_SUCCESS;
}

cpu_ops_t arch_cpu_ops = {
    .init = cpu_init_impl,
    .get_info = cpu_get_info_impl,
    .set_frequency = cpu_set_frequency_impl,
    .sleep_core = cpu_sleep_core_impl,
    .wakeup_core = cpu_wakeup_core_impl,
    .migrate_task = cpu_migrate_task_impl
};
