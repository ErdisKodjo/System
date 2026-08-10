/*
 * vector_table.S — Table de vecteurs d'interruptions ARM Cortex-M pour le
 * port MCU AfriOS.
 *
 * Format imposé par l'architecture Cortex-M : la première entrée est le SP
 * initial chargé par le hardware au reset, la deuxième est l'adresse du
 * reset handler. Toutes les exceptions système (NMI, HardFault, SVC, PendSV,
 * SysTick, …) suivent dans l'ordre fixe défini par ARMv7-M ARM §B1.5.
 *
 * Toutes les entrées sont weak-alias de Default_Handler sauf :
 *   - Reset_Handler (appelé en premier, saute à crt0 puis kernel_main)
 *   - SysTick_Handler (surchargeable par le timer_port.c pour le tick 1 ms)
 *
 * Aucune libc, aucune section .data initialisée — le symbole _stack_top est
 * fourni par le linker script (Kernel/hal/scripts/linker-mcu.ld).
 */

    .syntax unified
    .cpu cortex-m
    .thumb

/* -------------------------------------------------------------------------- */
/*  Symboles externes                                                          */
/* -------------------------------------------------------------------------- */
    .extern _stack_top          /* fourni par linker-mcu.ld (top of stack)     */
    .extern crt0_init           /* Kernel/ports/port-mcu/src/crt0.c            */
    .extern kernel_main         /* Kernel/afros/main.c                         */

/* -------------------------------------------------------------------------- */
/*  Section .isr_vector — chargée à l'adresse ORIGIN(FLASH) par linker-mcu.ld. */
/* -------------------------------------------------------------------------- */
    .section .isr_vector, "a", %progbits
    .align 2
    .global g_pfnVectors
    .type g_pfnVectors, %object

g_pfnVectors:
    .word _stack_top                /* 0:  Initial SP                            */
    .word Reset_Handler             /* 1:  Reset                                 */
    .word NMI_Handler               /* 2:  NMI                                   */
    .word HardFault_Handler         /* 3:  HardFault                             */
    .word MemManage_Handler         /* 4:  MemManage                             */
    .word BusFault_Handler          /* 5:  BusFault                              */
    .word UsageFault_Handler        /* 6:  UsageFault                            */
    .word 0                         /* 7:  Reserved                              */
    .word 0                         /* 8:  Reserved                              */
    .word 0                         /* 9:  Reserved                              */
    .word 0                         /* 10: Reserved                              */
    .word SVC_Handler               /* 11: SVCall                                */
    .word DebugMon_Handler          /* 12: Debug Monitor                         */
    .word 0                         /* 13: Reserved                              */
    .word PendSV_Handler            /* 14: PendSV                                */
    .word SysTick_Handler           /* 15: SysTick                               */

    /* Vecteurs 16+: IRQ externes (0..N-1). Sur Cortex-M, le nombre dépend du
     * cœur (M3=16, M4=82..110 selon vendor, M7=240). On réserve 16 entrées
     * génériques ici ; surcharger dans un fichier vendor-specific si besoin. */
    .word Default_Handler           /* IRQ 0  */
    .word Default_Handler           /* IRQ 1  */
    .word Default_Handler           /* IRQ 2  */
    .word Default_Handler           /* IRQ 3  */
    .word Default_Handler           /* IRQ 4  */
    .word Default_Handler           /* IRQ 5  */
    .word Default_Handler           /* IRQ 6  */
    .word Default_Handler           /* IRQ 7  */
    .word Default_Handler           /* IRQ 8  */
    .word Default_Handler           /* IRQ 9  */
    .word Default_Handler           /* IRQ 10 */
    .word Default_Handler           /* IRQ 11 */
    .word Default_Handler           /* IRQ 12 */
    .word Default_Handler           /* IRQ 13 */
    .word Default_Handler           /* IRQ 14 */
    .word Default_Handler           /* IRQ 15 */

    .size g_pfnVectors, . - g_pfnVectors

/* -------------------------------------------------------------------------- */
/*  Reset_Handler — première instruction exécutée. Doit être en Thumb (bit    */
/*  0 du PC = 1 attendu par Cortex-M), d'où le suffixe .Mask.                 */
/* -------------------------------------------------------------------------- */
    .section .text.Reset_Handler, "ax", %progbits
    .align 2
    .global Reset_Handler
    .type Reset_Handler, %function
    .thumb_func
Reset_Handler:
    /* Le hardware a déjà chargé SP depuis g_pfnVectors[0] (_stack_top).     */
    /* Initialiser le runtime C (copie .data, zéro .bss), puis kernel_main.  */
    bl      crt0_init
    bl      kernel_main
    /* kernel_main ne doit jamais revenir ; au cas où : boucle safe. */
.L_reset_loop:
    wfi
    b       .L_reset_loop
    .size Reset_Handler, . - Reset_Handler

/* -------------------------------------------------------------------------- */
/*  Default_Handler — implémentation par défaut de toutes les exceptions      */
/*  non surchargées. Weak : une surcharge dans un autre fichier .c/.S         */
/*  remplacera le symbole correspondant automatiquement.                      */
/* -------------------------------------------------------------------------- */
    .section .text.Default_Handler, "ax", %progbits
    .align 2
    .global Default_Handler
    .type Default_Handler, %function
    .thumb_func
Default_Handler:
    b       Default_Handler
    .size Default_Handler, . - Default_Handler

/* -------------------------------------------------------------------------- */
/*  Weak aliases — chaque handler nommé pointe par défaut sur Default_Handler.*/
/*  Une surcharge dans timer_port.c (SysTick_Handler) ou ailleurs prendra      */
/*  automatiquement le pas grâce au weak linkage.                             */
/* -------------------------------------------------------------------------- */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler

    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler

    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler

    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler

    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler

    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler

    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

    .end
