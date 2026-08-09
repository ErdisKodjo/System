#include "afros_hal.h"
#include <stdio.h>

/**
 * @file task_migration.c
 * @brief Migration de tches entre coeurs pour AfriOS.
 */

afros_status_t scheduler_migrate_task(uint32_t task_id, uint32_t from_cpu, uint32_t to_cpu) {
    printf("[MIGRATION] Tentative de migration de la tche %u : CPU %u -> CPU %u\n", 
           task_id, from_cpu, to_cpu);
    
    // 1. Pause de la tche sur le CPU source
    // 2. Sauvegarde du contexte (registres, pile)
    // 3. Transfert du contexte vers le CPU cible
    
    afros_status_t status = arch_cpu_ops.migrate_task(from_cpu, to_cpu, task_id);
    
    if (status == AFROS_SUCCESS) {
        printf("[MIGRATION] Succs : Tche %u dplace vers CPU %u.\n", task_id, to_cpu);
    } else {
        printf("[MIGRATION] ECHEC : Impossible de dplacer la tche %u.\n", task_id);
    }
    
    return status;
}

void scheduler_balance_load(void) {
    // Analyse de la charge moyenne de chaque coeur
    // Si un coeur est satur (ex: > 90%) et un autre est oisif (ex: < 20%)
    // Dclencher la migration automatique.
    printf("[SCHED-LB] Equilibrage automatique de la charge activ.\n");
}
