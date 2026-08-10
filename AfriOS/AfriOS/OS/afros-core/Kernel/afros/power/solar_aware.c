#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file solar_aware.c
 * @brief Gestionnaire d'énergie intelligent basé sur la source solaire pour AfriOS.
 */

void power_check_solar_status(void) {
    afros_power_source_t source;
    
    // 1. Détection de la source d'énergie via le HAL
    if (afros_hal_ops.get_power_source(&source) != AFROS_SUCCESS) return;
    
    // 2. Ajustement dynamique des performances
    if (source == AFROS_POWER_SOURCE_SOLAR) {
        kprintf("[POWER] Source solaire détectée. Activation du mode HAUTE PERFORMANCE.\n");
        kprintf("[POWER] Action : Fréquences CPU maximales autorisées.\n");
        
        // Exemple : Augmenter la fréquence du Core 4 (big)
        arch_cpu_ops.set_frequency(4, 2800);
    } else {
        kprintf("[POWER] Source Batterie/AC. Passage en mode ECONOMIE D'ENERGIE.\n");
        kprintf("[POWER] Action : Limitation des fréquences CPU.\n");
        
        // Exemple : Réduire la fréquence du Core 4
        arch_cpu_ops.set_frequency(4, 1800);
    }
}
