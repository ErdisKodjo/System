#include "cpu_abstraction.h"
#include <stdio.h>

/**
 * @file cpu_port.c
 * @brief Port MCU (Cortex-M) : mono-cœur, horloge fixe (PLL configurée au boot),
 *        pas de migration de tâche ni de clusters — un seul cpu_id valide (0).
 */

static afros_status_t cpu_init_impl(void) {
    printf("[CPU] MCU : cœur unique, PLL système déjà configurée par le startup code.\n");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_get_info_impl(uint32_t cpu_id, afros_cpu_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    if (cpu_id != 0) return AFROS_ERROR_INVALID_PARAM; // un seul cœur sur ce port

    info->core_id = 0;
    info->cluster_id = 0;
    info->is_big = false; // pas de notion big/LITTLE sur un MCU mono-cœur
    info->current_freq_mhz = 168; // ex. Cortex-M4 @168MHz, fixe

    return AFROS_SUCCESS;
}

static afros_status_t cpu_set_frequency_impl(uint32_t cpu_id, uint32_t freq_mhz) {
    (void)freq_mhz;
    if (cpu_id != 0) return AFROS_ERROR_INVALID_PARAM;
    // La plupart des MCU ne permettent pas de changer la fréquence à chaud
    // sans reconfigurer la PLL et risquer de désynchroniser les périphériques.
    printf("[CPU] MCU : changement de fréquence à chaud non supporté sur ce port.\n");
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t cpu_sleep_core_impl(uint32_t cpu_id) {
    if (cpu_id != 0) return AFROS_ERROR_INVALID_PARAM;
    printf("[CPU] MCU : instruction WFI (Wait For Interrupt), mode Sleep.\n");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_wakeup_core_impl(uint32_t cpu_id) {
    if (cpu_id != 0) return AFROS_ERROR_INVALID_PARAM;
    printf("[CPU] MCU : réveil par interruption NVIC (WFI se termine naturellement).\n");
    return AFROS_SUCCESS;
}

static afros_status_t cpu_migrate_task_impl(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id) {
    (void)from_cpu; (void)to_cpu; (void)task_id;
    // Aucune migration possible : un seul cœur.
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
