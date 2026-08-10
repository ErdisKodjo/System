#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file main.c
 * @brief Point d'entrée principal du noyau AfriOS.
 *
 * kernel_main() est l'entrée « réelle » du noyau : appelée soit par le
 * simulateur hébergé (main() ci-dessous, gardé par #ifndef AFROS_FREESTANDING),
 * soit par crt0.c sur le port MCU bare-metal. Aucune dépendance libc ici :
 * tout l'affichage passe par kprintf -> arch_console_ops (voir hal/src/kprintf.c).
 */

/*
 * Ces fonctions sont implémentées dans les modules dédiés du noyau
 * (scheduler/afros_cfs.c, power/solar_aware.c, memory/adaptive_reclaim.c) et
 * liées via la bibliothèque afros-kernel. main.c ne fait qu'orchestrer leur
 * appel à l'amorçage.
 */
void afros_cfs_init(void);
void afros_cfs_run(void);
void power_check_solar_status(void);
void memory_reclaim_pages(uint32_t threshold_percentage);

void kernel_main(void) {
    kprintf("--- Bienvenue dans AfriOS Core (ARM64 v8.5) ---\n");
    kprintf("[KERNEL] Démarrage du système d'amorçage...\n\n");

    /* 1. Initialisation de la HAL (console -> CPU -> mémoire -> IRQ -> timer -> storage). */
    if (afros_hal_ops.init() != AFROS_SUCCESS) {
        kprintf("[KERNEL] ERREUR FATALE : Echec HAL\n");
        return;
    }

    /* 2. Vérification de l'alimentation (optimisation solaire). */
    power_check_solar_status();

    /* 3. Initialisation de l'ordonnanceur CFS. */
    afros_cfs_init();

    /* 4. Passage en mode opérationnel. */
    kprintf("\n[KERNEL] Noyau opérationnel. Lancement du planificateur...\n");
    afros_cfs_run();

    /* 5. Démonstration de la gestion mémoire adaptative. */
    kprintf("\n[KERNEL] Démonstration de la gestion mémoire...\n");
    memory_reclaim_pages(90); /* simule une forte utilisation mémoire pour déclencher la récupération */

    /* 6. Boucle de sécurité (idle loop). */
    kprintf("[KERNEL] Arrêt du système ou Idle...\n");
    while (1) {
        /* idle : en bare-metal, WFI/hlt serait déclenché par le port CPU. */
    }
}

/*
 * Point d'entrée du simulateur hébergé uniquement. Le build MCU définit
 * AFROS_FREESTANDING et fournit son propre point d'entrée (vector_table.S ->
 * crt0.c -> kernel_main), donc main() est masqué dans ce cas pour éviter le
 * conflit de symboles.
 */
#ifndef AFROS_FREESTANDING
int main(void) {
    kernel_main();
    return 0;
}
#endif
