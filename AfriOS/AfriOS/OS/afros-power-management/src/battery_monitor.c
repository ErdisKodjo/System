#include "../include/afros_power.h"
#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file battery_monitor.c
 * @brief Surveillant de batterie avec gestion intelligente pour AfriOS.
 *
 * Freestanding : utilise kprintf (HAL) au lieu de <stdio.h>/printf. Les
 * accès à afros_hal_ops (get_battery_level, get_power_source) viennent de
 * afros-hal/src/hal_init.c — la cible CMake afros-hal est liée.
 */

afros_status_t power_init(void) {
    kprintf("[POWER-MGR] Initializing power management subsystem...\n");
    return AFROS_SUCCESS;
}

void power_monitor_battery(void) {
    uint32_t level;
    afros_power_source_t source;

    /* Accès aux fonctions HAL de afros-core (target afros-hal linkée). */
    afros_hal_ops.get_battery_level(&level);
    afros_hal_ops.get_power_source(&source);

    kprintf("[POWER-MGR] Monitoring batterie...\n");
    kprintf("[POWER-MGR] Source : %s, Niveau : %u%%\n",
            (source == AFROS_POWER_SOURCE_SOLAR ? "Solaire" : "Batterie"), level);

    if (level < 15 && source != AFROS_POWER_SOURCE_SOLAR) {
        kprintf("[POWER-MGR] Alerte : Niveau critique. Activation du profil d'economie d'energie.\n");
    }
}
