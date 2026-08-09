#include "afros_hal.h"
#include <stdio.h>

/**
 * @file big_little.c
 * @brief Extension de l'ordonnanceur pour le support big.LITTLE dans AfriOS.
 */

void scheduler_check_migration(uint32_t task_id, uint32_t current_cpu_id, uint32_t load_percentage) {
    afros_cpu_info_t info;
    
    // 1. Récupération des informations du CPU actuel via le HAL
    if (arch_cpu_ops.get_info(current_cpu_id, &info) != AFROS_SUCCESS) return;
    
    // 2. Analyse de la charge et de la topologie
    if (!info.is_big && load_percentage > 80) {
        printf("[big.LITTLE] Alerte : Tâche %u sur CPU %u (LITTLE) surchargée (%u%%).\n", task_id, current_cpu_id, load_percentage);
        
        // Suggestion de migration vers un coeur big (ex: Core 4)
        uint32_t target_cpu = 4;
        printf("[big.LITTLE] Action : Migration demandée de %u vers %u.\n", current_cpu_id, target_cpu);
        
        arch_cpu_ops.migrate_task(current_cpu_id, target_cpu, task_id);
    } else {
        printf("[big.LITTLE] État : Équilibre optimal pour la tâche %u sur Core %u.\n", task_id, current_cpu_id);
    }
}
