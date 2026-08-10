/*
 * crt0.c — runtime C minimal pour le port MCU AfriOS (ARM Cortex-M).
 *
 * Appelé par Reset_Handler (vector_table.S) AVANT kernel_main. Responsable de:
 *   1. copier la section .data depuis la flash (load address _sidata) vers la
 *      SRAM (runtime address _sdata.._edata) ;
 *   2. mettre à zéro la section .bss (_sbss.._ebss) ;
 *   3. appeler kernel_main().
 *
 * Ce fichier ne doit dépendre d'aucune libc : pas de memset/memcpy (on utilise
 * des boucles locales) et la fonction est marquée `naked` sur l'entrée Reset
 * (le prologue/épilogue par défaut du compilateur n'est pas souhaitable tant
 * que .data/.bss ne sont pas initialisés — on évite toute utilisation de
 * variable globale avant l'init).
 *
 * Les symboles _sidata / _sdata / _edata / _sbss / _ebss / _stack_top sont
 * fournis par Kernel/hal/scripts/linker-mcu.ld.
 */

#include <stdint.h>
#include <stddef.h>

/* Symboles définis par le linker script. */
extern uint32_t _sidata;   /* load address of .data (en flash)            */
extern uint32_t _sdata;    /* runtime start of .data (en SRAM)            */
extern uint32_t _edata;    /* runtime end of .data (en SRAM)              */
extern uint32_t _sbss;     /* start of .bss (en SRAM)                     */
extern uint32_t _ebss;     /* end of .bss (en SRAM)                       */
/* _stack_top est utilisé par le hardware (vecteur[0]), pas besoin ici. */

/* kernel_main est définie dans Kernel/afros/main.c. */
extern void kernel_main(void);

/**
 * @brief Initialise le runtime C : copie .data flash->RAM, zéro .bss.
 *
 * Appelée par Reset_Handler avant kernel_main. Marquée pour ne pas dépendre
 * d'une stack déjà valide (le hardware a déjà chargé SP depuis le vecteur 0).
 */
void crt0_init(void) {
    /* 1. Copier .data depuis la flash (_sidata) vers la SRAM (_sdata.._edata).
     *    On copie par mots de 32 bits pour aligner avec la convention ARM. */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* 2. Zéroter .bss (_sbss.._ebss) — variables globales non initialisées. */
    for (uint32_t *p = &_sbss; p < &_ebss; ) {
        *p++ = 0u;
    }

    /* 3. Appeler kernel_main — ne doit jamais revenir. */
    kernel_main();

    /* 4. Ce point n'est atteint que si kernel_main revient (ne devrait pas).
     *    Boucle safe pour éviter de revenir à une adresse indéfinie. */
    for (;;) {
#if defined(__arm__) || defined(__thumb__)
        __asm__ volatile ("wfi");   /* Wait For Interrupt (Cortex-M) */
#else
        __asm__ volatile ("hlt");   /* Repli x86 pour la syntaxe-check cross-build */
#endif
    }
}
