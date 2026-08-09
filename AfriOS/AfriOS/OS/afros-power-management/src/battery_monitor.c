#include "../include/afros_power.h"
#include "../../afros-core/Kernel/hal/include/afros_hal.h"
#include <stdio.h>

/**
 * @file battery_monitor.c
 * @brief Surveillant de batterie avec gestion intelligente pour AfriOS.
 */

afros_status_t power_init(void) {
    printf("[POWER-MGR] Initializing power management subsystem...\n");
    return AFROS_SUCCESS;
}

void power_monitor_battery(void) {
    uint32_t level;
    afros_power_source_t source;
    
    // Acc�s aux fonctions HAL de afros-core
    afros_hal_ops.get_battery_level(&level);
    afros_hal_ops.get_power_source(&source);
    
    printf("[POWER-MGR] Monitoring batterie...\n");
    printf("[POWER-MGR] Source : %s, Niveau : %u%%\n", (source == AFROS_POWER_SOURCE_SOLAR ? "Solaire" : "Batterie"), level);
    
    if (level < 15 && source != AFROS_POWER_SOURCE_SOLAR) {
        printf("[POWER-MGR] Alerte : Niveau critique. Activation du profil d'�conomie d'�nergie.\n");
    }
}
