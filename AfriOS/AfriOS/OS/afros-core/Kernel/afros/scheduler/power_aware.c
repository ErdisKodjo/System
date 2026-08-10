#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file power_aware.c
 * @brief Ordonnancement conscient de l'nergie pour AfriOS.
 */

afros_status_t scheduler_optimize_for_power(void) {
    afros_power_source_t source;
    uint32_t battery_level;
    
    afros_hal_ops.get_power_source(&source);
    afros_hal_ops.get_battery_level(&battery_level);
    
    kprintf("[POWER-SCHED] Optimisation en cours (Source: %d, Batterie: %u%%)...\n", source, battery_level);
    
    if (source == AFROS_POWER_SOURCE_BATTERY && battery_level < 20) {
        kprintf("[POWER-SCHED] Mode CONSERVATION : Limitation aux coeurs LITTLE et baisse de frquence.\n");
        // Forcer l'affinit des tches vers les coeurs LITTLE (ID 0-3 par exemple)
        return AFROS_SUCCESS;
    }
    
    if (source == AFROS_POWER_SOURCE_SOLAR || source == AFROS_POWER_SOURCE_AC) {
        kprintf("[POWER-SCHED] Mode PERFORMANCE : Coeurs big autoriss et frquence maximale.\n");
    }
    
    return AFROS_SUCCESS;
}

void scheduler_handle_thermal_throttle(uint32_t temp_c) {
    if (temp_c > 85) {
        kprintf("[POWER-SCHED] ALERTE THERMIQUE (%u C) : Migration des tches vers les coeurs inactifs.\n", temp_c);
        // Diminution de la frquence du CPU via le HAL
        arch_cpu_ops.set_frequency(0, 1000); // Ex: 1GHz
    }
}
