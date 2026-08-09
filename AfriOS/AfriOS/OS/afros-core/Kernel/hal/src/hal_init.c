#include "afros_hal.h"
#include <stdio.h>

/**
 * @file hal_init.c
 * @brief Implementation principale de l'initialisation du HAL pour AfriOS.
 */

extern cpu_ops_t arch_cpu_ops;
extern memory_ops_t arch_memory_ops;
extern interrupt_ops_t arch_interrupt_ops;
extern timer_ops_t arch_timer_ops;
extern console_ops_t arch_console_ops;

static afros_status_t hal_init_impl(void) {
    // La console est initialisée en premier : c'est le seul canal de sortie
    // disponible si une étape suivante échoue.
    if (arch_console_ops.init(115200) != AFROS_SUCCESS) {
        return AFROS_ERROR;
    }

    printf("[HAL] Initialisation de la couche d'abstraction...\n");

    // 1. Initialisation CPU
    if (arch_cpu_ops.init() != AFROS_SUCCESS) {
        printf("[HAL] Erreur fatale: Echec initialisation CPU\n");
        return AFROS_ERROR;
    }

    // 2. Initialisation Memoire
    if (arch_memory_ops.init() != AFROS_SUCCESS) {
        printf("[HAL] Erreur fatale: Echec initialisation Memoire\n");
        return AFROS_ERROR;
    }

    // 3. Initialisation du contrôleur d'interruptions (masquées jusqu'à ce
    //    que le noyau ait enregistré ses handlers - afros_kernel s'en charge).
    if (arch_interrupt_ops.init() != AFROS_SUCCESS) {
        printf("[HAL] Erreur fatale: Echec initialisation IRQ\n");
        return AFROS_ERROR;
    }

    // 4. Initialisation du timer système (100 Hz par défaut)
    if (arch_timer_ops.init(100) != AFROS_SUCCESS) {
        printf("[HAL] Erreur fatale: Echec initialisation Timer\n");
        return AFROS_ERROR;
    }

    printf("[HAL] Syst�me pr�t pour le noyau.\n");
    return AFROS_SUCCESS;
}

static afros_status_t hal_get_power_source_impl(afros_power_source_t *source) {
    if (!source) return AFROS_ERROR_INVALID_PARAM;
    // Simulation d'une source solaire pour AfriOS
    *source = AFROS_POWER_SOURCE_SOLAR;
    return AFROS_SUCCESS;
}

hal_ops_t afros_hal_ops = {
    .init = hal_init_impl,
    .reset = NULL,
    .shutdown = NULL,
    .suspend = NULL,
    .resume = NULL,
    .get_power_source = hal_get_power_source_impl,
    .get_battery_level = NULL
};
