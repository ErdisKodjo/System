#include "cpu_abstraction.h"
#include "kprintf.h"

/**
 * @file cpu_port.c
 * @brief Port ARM64 : CPU HAL, cluster big.LITTLE (ARMv8/v9).
 *        Déplacé depuis hal/src/cpu_manager.c (étape 3) : c'était la seule
 *        implémentation de arch_cpu_ops, codée en dur pour ARM64 alors que
 *        hal/ doit rester agnostique de l'architecture.
 */

static afros_status_t cpu_init_impl(void) {
    kprintf("[CPU] Initialisation de l'ordonnancement clusteris� (ARM v8/v9 aware)...\n");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_get_info_impl(uint32_t cpu_id, afros_cpu_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    
    // Simulation 8 coeurs : 4 LITTLE (0-3), 4 big (4-7)
    info->core_id = cpu_id;
    info->cluster_id = (cpu_id < 4) ? 0 : 1; 
    info->is_big = (cpu_id >= 4);
    info->current_freq_mhz = (info->is_big) ? 2800 : 1400;
    
    return AFROS_SUCCESS;
}

static afros_status_t cpu_set_frequency_impl(uint32_t cpu_id, uint32_t freq_mhz) {
    kprintf("[CPU] Core %u : Fr�quence fix�e � %u MHz.\n", cpu_id, freq_mhz);
    return AFROS_SUCCESS;
}

static afros_status_t cpu_migrate_task_impl(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id) {
    kprintf("[CPU] Migration : T�che %u d�plac�e du Core %u (Source) vers le Core %u (Dest).\n", task_id, from_cpu, to_cpu);
    return AFROS_SUCCESS;
}

cpu_ops_t arch_cpu_ops = {
    .init = cpu_init_impl,
    .get_info = cpu_get_info_impl,
    .set_frequency = cpu_set_frequency_impl,
    .sleep_core = NULL,
    .wakeup_core = NULL,
    .migrate_task = cpu_migrate_task_impl
};
