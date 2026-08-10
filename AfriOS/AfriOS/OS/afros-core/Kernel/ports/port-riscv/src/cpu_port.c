#include "cpu_abstraction.h"
#include "kprintf.h"

/**
 * @file cpu_port.c
 * @brief Port RISC-V : harts (RV64GC), identification via mhartid/misa/mvendorid.
 *        Pas de notion big.LITTLE standardisée : cœurs supposés homogènes sauf
 *        indication contraire du Device Tree (cpu-map).
 */

static afros_status_t cpu_init_impl(void) {
    kprintf("[CPU] RISC-V : lecture de misa/mvendorid/marchid pour chaque hart...\n");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_get_info_impl(uint32_t cpu_id, afros_cpu_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;

    // Harts homogènes par défaut (cluster unique) - un SoC hétérogène (ex: SiFive
    // U84+E-cores) surchargerait cluster_id/is_big depuis le Device Tree.
    info->core_id = cpu_id;      // = mhartid
    info->cluster_id = 0;
    info->is_big = true;
    info->current_freq_mhz = 1400;

    return AFROS_SUCCESS;
}

static afros_status_t cpu_set_frequency_impl(uint32_t cpu_id, uint32_t freq_mhz) {
    kprintf("[CPU] RISC-V : hart %u -> appel SBI (extension DVFS spécifique plateforme) pour %u MHz.\n", cpu_id, freq_mhz);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_sleep_core_impl(uint32_t cpu_id) {
    kprintf("[CPU] RISC-V : hart %u -> instruction WFI.\n", cpu_id);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_wakeup_core_impl(uint32_t cpu_id) {
    kprintf("[CPU] RISC-V : hart %u -> SBI HSM hart_start / IPI CLINT.\n", cpu_id);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_migrate_task_impl(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id) {
    kprintf("[CPU] RISC-V : tâche %u migrée du hart %u vers le hart %u.\n", task_id, from_cpu, to_cpu);
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
