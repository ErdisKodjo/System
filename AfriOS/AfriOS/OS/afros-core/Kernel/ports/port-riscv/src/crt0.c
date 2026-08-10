/*
 * crt0.c — runtime C minimal pour le port RISC-V AfriOS (RV64, S-mode).
 *
 * Appelé par riscv_reset (vector_table.S) AVANT kernel_main. Responsable de :
 *   1. (pas de copie .data flash→RAM : sur QEMU virt RISC-V le noyau est
 *      chargé directement en RAM à 0x80200000 par OpenSBI, .data est donc
 *      déjà à sa runtime address. Le linker script linker-riscv.ld ne
 *      définit d'ailleurs pas _sidata — l'omet ici.)
 *   2. mettre à zéro la section .bss (_sbss.._ebss) ;
 *   3. installer la trap vector table (CSR stvec ← riscv_trap_vector_table) ;
 *   4. activer les interruptions S-mode (SIE) — optionnel, kernel_main peut
 *      le faire lui-même via arch_interrupt_ops.enable_interrupts() ;
 *      on garde l'init passive par défaut pour laisser le port décider.
 *   5. appeler kernel_main().
 *
 * Ce fichier ne doit dépendre d'aucune libc : pas de memset/memcpy (on utilise
 * des boucles locales). SP et GP ont déjà été initialisés par riscv_reset
 * avant l'appel à crt0_init — on peut donc utiliser des variables locales
 * et des symboles globals (.sdata, .sbss).
 *
 * Les symboles _sdata / _edata / _sbss / _ebss / _stack_top sont fournis
 * par Kernel/hal/scripts/linker-riscv.ld.
 *
 * Mode : S-mode. OpenSBI (firmware M-mode) a déjà configuré :
 *   - PMP (le noyau a accès à toute la RAM),
 *   - la MMU (en mode Bare pour le noyau ; le noyau peut activer Sv39/Sv48
 *     lui-même via arch_memory_ops.map),
 *   - le CLINT et le PLIC sont accessibles en MMIO,
 *   - la console SBI (ecall DBCN) est disponible.
 */

#include <stdint.h>
#include <stddef.h>

/* Symboles définis par le linker script linker-riscv.ld. */
extern uint8_t _sdata;        /* runtime start of .data (en RAM, déjà chargé) */
extern uint8_t _edata;        /* runtime end   of .data                       */
extern uint8_t _sbss;         /* start of .bss                                */
extern uint8_t _ebss;         /* end   of .bss                                */
/* _stack_top et __global_pointer$ sont chargés par riscv_reset avant l'appel
 * à crt0_init ; pas besoin de les référencer ici. */

/* Trap vector table — définie dans vector_table.S. */
extern void riscv_trap_vector_table(void);

/* kernel_main est définie dans Kernel/afros/main.c. */
extern void kernel_main(void);

/* RISC-V privileged CSR access helpers — inlined asm pour éviter une
 * dépendance à une libc ou un compilateur spécifique. */
static inline void csr_write_stvec(uintptr_t value) {
    __asm__ volatile ("csrw stvec, %0" :: "r"(value) : "memory");
}

/**
 * @brief Initialise le runtime C pour RISC-V S-mode.
 *
 * Étapes :
 *   1. (skip) pas de copie .data : déjà en RAM.
 *   2. Zéroter .bss (_sbss.._ebss) — variables globales non initialisées.
 *   3. Installer la trap vector table (mode vectored : stvec = base | 0b01).
 *   4. Appeler kernel_main — ne doit jamais revenir.
 *
 * Appelée par riscv_reset avant kernel_main.
 */
void crt0_init(void) {
    /* 1. (pas de copie .data — déjà en RAM sur QEMU virt RISC-V) */

    /* 2. Zéroter .bss (_sbss.._ebss). On byte-loop pour rester indépendant
     *    de l'alignement de .bss (le linker script aligne sur 8 mais on
     *    ne veut pas coder cette hypothèse en dur ici). */
    for (uint8_t *p = &_sbss; p < &_ebss; ) {
        *p++ = 0u;
    }

    /* 3. Installer la trap vector table.
     *    stvec[1:0] = mode : 0b00 = direct (1 handler unique), 0b01 = vectored
     *    (table de N entrées). On utilise le mode vectored — voir le header
     *    de vector_table.S pour la justification.
     *    L'adresse de la table doit être alignée 4-byte (XLEN/8 pour RV64
     *    serait 8, mais RISC-V Privileged ISA §3.1.7 impose seulement 4-byte
     *    alignment pour les bits [1:0] du mode).
     *
     *    NB : le linker script section .trap_vector est .align 2 (=4-byte).
     *    On force l'alignement ici par masking des 2 bas bits, qu'on remet
     *    à 0b01 pour le mode vectored. */
    uintptr_t tvec = (uintptr_t)&riscv_trap_vector_table;
    tvec = (tvec & ~(uintptr_t)0x3) | 0x1;   /* vectored mode */
    csr_write_stvec(tvec);

    /* 4. Appeler kernel_main — ne doit jamais revenir. */
    kernel_main();

    /* 5. Ce point n'est atteint que si kernel_main revient (ne devrait pas).
     *    Boucle safe : wfi économise l'énergie en attendant une interruption
     *    qui ne viendra jamais (SIE pas encore activé), donc le hart s'arrête
     *    proprement. */
    for (;;) {
        __asm__ volatile ("wfi");
    }
}
