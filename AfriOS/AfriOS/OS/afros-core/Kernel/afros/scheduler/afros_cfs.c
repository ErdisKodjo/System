#include "afros_hal.h"
#include <stdio.h>

/**
 * @file afros_cfs.c
 * @brief Ordonnanceur Completement Equitable (CFS) pour AfriOS.
 */

void scheduler_check_migration(uint32_t task_id, uint32_t current_cpu_id, uint32_t load_percentage);

void afros_cfs_init(void) {
    printf("[SCHED] Initialisation de l'ordonnanceur CFS...\n");
}

void afros_cfs_run(void) {
    printf("[SCHED] Cycle d'ordonnancement actif.\n");
    
    // Simulation d'une boucle de monitoring
    uint32_t task_demo = 202;
    uint32_t cpu_demo = 1;   // Core LITTLE
    uint32_t load_demo = 88; // Charge élevée
    
    printf("[SCHED] Monitoring : Task %u sur Core %u, Charge: %u%%\n", task_demo, cpu_demo, load_demo);
    
    // Appel à la logique big.LITTLE
    scheduler_check_migration(task_demo, cpu_demo, load_demo);
}
