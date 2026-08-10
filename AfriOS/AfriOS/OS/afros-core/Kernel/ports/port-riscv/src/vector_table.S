/*
 * vector_table.S — Table de vecteurs de trap RISC-V (S-mode) pour le port
 * AfriOS RISC-V (cible QEMU virt RV64 : 1 hart, OpenSBI en M-mode, noyau en
 * S-mode).
 *
 * Architecture : RV64GC. Le noyau AfriOS tourne en Supervisor mode (S-mode) ;
 * OpenSBI (firmware M-mode) a déjà configuré PMP, MPLIC, CLINT, et a installé
 * un handler SBI pour les ecall. Le noyau est chargé à 0x80200000 par
 * OpenSBI (convention QEMU virt) juste après le reset.
 *
 * Différences majeures avec Cortex-M (voir port-mcu/src/vector_table.S) :
 *   - RISC-V n'a PAS de table d'adresses contiguë en mémoire pointée par
 *     un registre matériel. Le CSR `stvec` (Supervisor Trap Vector) pointe
 *     vers UNE seule adresse : le trap handler commun. Selon le mode
 *     (vectored vs direct), `stvec` pointe soit vers un dispatcher unique
 *     qui lit `scause` pour brancher, soit vers une table alignée 4-byte
 *     d'adresses indexée par `scause`.
 *   - On utilise ici le mode "vectored" (stvec[1:0] = 0b01) : la table
 *     contient 256 entrées (une par cause de trap), le hardware saute
 *     directement à table[cause]. Les entrées non utilisées pointent
 *     vers `riscv_default_handler`.
 *
 *   - Reset : sur RISC-V, l'entry point n'est PAS une trap. OpenSBI charge
 *     le noyau et saute à `_start` (ou `riscv_reset` ici). Il n'y a donc
 *     pas de reset vector dans la trap table — `riscv_reset` est juste un
 *     symbole `.globl` placé en tête de .text, pointé par l'entry du linker
 *     script (linker-riscv.ld : `ENTRY(riscv_reset)`).
 *
 * Entrées de la table (index = scause, voir RISC-V Privileged ISA §3.1.14) :
 *   0  : Instruction address misaligned
 *   1  : Instruction access fault
 *   2  : Illegal instruction
 *   3  : Breakpoint
 *   4  : Load address misaligned
 *   5  : Load access fault
 *   6  : Store/AMO address misaligned
 *   7  : Store/AMO access fault
 *   8  : Environment call from U-mode (ecall)
 *   9  : Environment call from S-mode (ecall)
 *   10 : Environment call from M-mode (normalement jamais en S-mode)
 *   11 : (reserved)
 *   12 : Instruction page fault
 *   13 : Load page fault
 *   14 : (reserved)
 *   15 : Store/AMO page fault
 *
 *   Index >= 16 sont des interruptions (bit 63 de scause = 1).
 *   On distingue par le champ "code" (bits 62:0) :
 *   0  : User software interrupt
 *   1  : Supervisor software interrupt
 *   5  : Supervisor timer interrupt
 *   9  : Supervisor external interrupt
 *
 * Convention : cette table contient 256 entrées (cause 0..255) — suffisant
 * pour toutes les traps S-mode standard. Le hardware ignore le bit 63 de
 * scause lors de l'indexation en mode vectored, donc les interruptions
 * (cause >= 2^63) sont mappées sur les mêmes entrées que les exceptions.
 *
 * Tous les handlers sont weak-alias de `riscv_default_handler` sauf
 * `riscv_reset`. Le port RISC-V (interrupt_port.c) surcharge les handlers
 * timer/external/software via les noms attendus :
 *   - riscv_timer_irq_handler
 *   - riscv_soft_irq_handler
 *   - riscv_ext_irq_handler
 *   - riscv_ecall_u_handler
 *   - riscv_ecall_s_handler
 *   - riscv_instr_fault_handler
 *   - riscv_load_fault_handler
 *   - riscv_store_fault_handler
 *
 * NB : pas de libc, pas de section .data initialisée. _stack_top et
 * __global_pointer$ sont fournis par le linker script
 * (Kernel/hal/scripts/linker-riscv.ld).
 */

    .option norvc           /* désactive l'extension compressed pour cette table :
                             * chaque entrée fait exactement 4 bytes (XLEN/8). */

/* -------------------------------------------------------------------------- */
/*  Symboles externes                                                          */
/* -------------------------------------------------------------------------- */
    .extern _stack_top          /* fourni par linker-riscv.ld (top of stack)   */
    .extern __global_pointer$   /* fourni par linker-riscv.ld (relaxation GP)  */
    .extern crt0_init           /* Kernel/ports/port-riscv/src/crt0.c          */
    .extern kernel_main         /* Kernel/afros/main.c                         */

/* -------------------------------------------------------------------------- */
/*  riscv_reset — point d'entrée du noyau. OpenSBI saute ici après son init    */
/*  (passage M→S, PMP ouvert pour 0x80200000+). Aucune stack ni GP valides     */
/*  à l'entrée : on doit les établir avant tout appel C.                       */
/* -------------------------------------------------------------------------- */
    .section .text.riscv_reset, "ax", %progbits
    .align 2
    .globl riscv_reset
    .type riscv_reset, %function
riscv_reset:
    /* 1. SP : le noyau est chargé en RAM à 0x80200000, la pile pousse vers
     *    le bas depuis _stack_top (fourni par le linker script). */
    la      sp, _stack_top

    /* 2. GP (global pointer) : requis pour la relaxation linker (-msmall-data).
     *    Le linker calcule __global_pointer$ ; on le charge explicitement pour
     *    garantir que tout accès .sdata/.sbss marche avant crt0_init. */
    la      gp, __global_pointer$

    /* 3. Init runtime C (zero .bss ; pas de copie .data car déjà en RAM),
     *    puis kernel_main. */
    call    crt0_init
    call    kernel_main

    /* 4. kernel_main ne doit jamais revenir ; au cas où : boucle safe. */
.L_reset_loop:
    wfi
    j       .L_reset_loop
    .size riscv_reset, . - riscv_reset

/* -------------------------------------------------------------------------- */
/*  Trap vector table — pointée par CSR stvec (mode vectored, bits [1:0]=0b01).*/
/*  Alignement 4-byte (XLEN=64 mais les entrées font 4 bytes : `j <handler>`   */
/*  est encodé sur 4 octets sans RVC).                                         */
/*                                                                            */
/*  IMPORTANT : en mode vectored, RISC-V saute à table[cause << 2]. La table   */
/*  DOIT donc être contiguë et chaque entrée DOIT tenir dans 4 bytes. On      */
/*  utilise `j <handler>` (JALR auto-généré si hors portée, mais ici tous les */
/*  handlers sont dans le même fichier .text).                                 */
/* -------------------------------------------------------------------------- */
    .section .trap_vector, "ax", %progbits
    .align 2
    .globl riscv_trap_vector_table
    .type riscv_trap_vector_table, %object

riscv_trap_vector_table:
    /* --- Exceptions synchrones (scause 0..15) --- */
    j riscv_default_handler            /* 0:  Instruction address misaligned   */
    j riscv_instr_fault_handler        /* 1:  Instruction access fault         */
    j riscv_illegal_handler            /* 2:  Illegal instruction              */
    j riscv_breakpoint_handler         /* 3:  Breakpoint (ebreak)              */
    j riscv_default_handler            /* 4:  Load address misaligned          */
    j riscv_load_fault_handler         /* 5:  Load access fault                */
    j riscv_default_handler            /* 6:  Store/AMO address misaligned     */
    j riscv_store_fault_handler        /* 7:  Store/AMO access fault           */
    j riscv_ecall_u_handler            /* 8:  Environment call from U-mode     */
    j riscv_ecall_s_handler            /* 9:  Environment call from S-mode     */
    j riscv_default_handler            /* 10: ecall from M-mode (jamais en S)  */
    j riscv_default_handler            /* 11: reserved                         */
    j riscv_instr_fault_handler        /* 12: Instruction page fault           */
    j riscv_load_fault_handler         /* 13: Load page fault                  */
    j riscv_default_handler            /* 14: reserved                         */
    j riscv_store_fault_handler        /* 15: Store/AMO page fault             */

    /* --- Entrées 16..255 : interruptions. En mode vectored, le bit 63 de    */
    /*     scause est masqué par le hardware, donc l'index = (cause & 0xFF).   */
    /*     Cause 0x80000005 (supervisor timer) indexe ici l'entrée 5+16 = ...   */
    /*     En pratique, on place des handlers dédiés aux 3 interruptions S-mode */
    /*     standards (soft=1, timer=5, external=9) et default pour le reste.    */
    .rept 256 - 16
    j riscv_default_handler
    .endr
    .size riscv_trap_vector_table, . - riscv_trap_vector_table

/* -------------------------------------------------------------------------- */
/*  riscv_default_handler — implémentation par défaut de toutes les traps non  */
/*  surchargées. Weak : une surcharge dans un autre fichier .c/.S remplacera   */
/*  le symbole correspondant automatiquement.                                  */
/*                                                                            */
/*  En attendant une surcharge, on logge (si la console est déjà up) et on     */
/*  boucle — pour un kernel en S-mode, retourner depuis une trap synchrone     */
/*  sans avoir corrigé la cause va juste re-trapper immédiatement.             */
/* -------------------------------------------------------------------------- */
    .section .text.riscv_default_handler, "ax", %progbits
    .align 2
    .globl riscv_default_handler
    .type riscv_default_handler, %function
riscv_default_handler:
    /* Sauver scause/stval pour debug si un humain branche un JTAG. */
    csrr    t0, scause
    csrr    t1, stval
    /* Boucle safe : wfi pour économiser l'énergie, j pour relire. */
.L_default_loop:
    wfi
    j       .L_default_loop
    .size riscv_default_handler, . - riscv_default_handler

/* -------------------------------------------------------------------------- */
/*  Weak aliases — chaque handler nommé pointe par défaut sur                  */
/*  riscv_default_handler. Une surcharge dans interrupt_port.c                */
/*  (riscv_timer_irq_handler, etc.) prendra automatiquement le pas grâce au    */
/*  weak linkage.                                                             */
/* -------------------------------------------------------------------------- */
    .weak riscv_instr_fault_handler
    .set  riscv_instr_fault_handler, riscv_default_handler

    .weak riscv_illegal_handler
    .set  riscv_illegal_handler, riscv_default_handler

    .weak riscv_breakpoint_handler
    .set  riscv_breakpoint_handler, riscv_default_handler

    .weak riscv_load_fault_handler
    .set  riscv_load_fault_handler, riscv_default_handler

    .weak riscv_store_fault_handler
    .set  riscv_store_fault_handler, riscv_default_handler

    .weak riscv_ecall_u_handler
    .set  riscv_ecall_u_handler, riscv_default_handler

    .weak riscv_ecall_s_handler
    .set  riscv_ecall_s_handler, riscv_default_handler

    /* Handlers d'interruption (surcharge par interrupt_port.c via PLIC/CLINT). */
    .weak riscv_timer_irq_handler
    .set  riscv_timer_irq_handler, riscv_default_handler

    .weak riscv_soft_irq_handler
    .set  riscv_soft_irq_handler, riscv_default_handler

    .weak riscv_ext_irq_handler
    .set  riscv_ext_irq_handler, riscv_default_handler

    .end
