#include "afros_hal.h"
#include <stdio.h>

/**
 * @file main.c
 * @brief Point d'entr�e principal du noyau AfriOS.
 */

/*
 * Ces fonctions sont implmentes dans les modules ddis du noyau
 * (scheduler/afros_cfs.c, power/solar_aware.c, memory/adaptive_reclaim.c) et
 * lies via la bibliothque afros-kernel. main.c ne fait qu'orchestrer leur
 * appel  l'amorage.
 */
void afros_cfs_init(void);
void afros_cfs_run(void);
void power_check_solar_status(void);
void memory_reclaim_pages(uint32_t threshold_percentage);

void kernel_main(void) {
    printf("--- Bienvenue dans AfriOS Core (ARM64 v8.5) ---\n");
    printf("[KERNEL] D�marrage du syst�me de d�marrage...\n\n");

    // 1. Initialisation de la HAL
    if (afros_hal_ops.init() != AFROS_SUCCESS) {
        printf("[KERNEL] ERREUR FATALE : Echec HAL\n");
        return;
    }

    // 2. V�rification de l'alimentation (Optimisation Solaire)
    power_check_solar_status();

    // 3. Initialisation de l'ordonnanceur CFS
    afros_cfs_init();

    // 4. Passage en mode op�rationnel
    printf("\n[KERNEL] Noyau op�rationnel. Lancement du planificateur...\n");
    afros_cfs_run();

    // 5. Dmonstration de la gestion de mmoire adaptative
    printf("\n[KERNEL] Dmonstration de la gestion mmoire...\n");
    memory_reclaim_pages(90); // Simule une forte utilisation mmoire pour dclencher la rcupation

    // 5. Boucle de s�curit� (Idle loop)
    printf("[KERNEL] Arr�t du syst�me ou Idle...\n");
    while(1);
}

// Fonction utilitaire pour la d�monstration (appel�e par certains modules)
int main() {
    kernel_main();
    return 0;
}
