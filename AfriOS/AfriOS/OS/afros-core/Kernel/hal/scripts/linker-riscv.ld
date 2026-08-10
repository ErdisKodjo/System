/*
 * linker-riscv.ld — Linker script pour le port RISC-V AfriOS (RV64, S-mode).
 *
 * Cible : QEMU virt RISC-V (qemu-system-riscv64 -M virt).
 *   - OpenSBI est chargé à 0x80000000 par QEMU (firmware M-mode).
 *   - OpenSBI charge le noyau AfriOS à 0x80200000 (convention QEMU virt,
 *     cf. qemu/hw/riscv/virt.c : load_kernel_at = 0x80200000).
 *   - Le noyau tourne en S-mode, PMP pré-ouvert par OpenSBI pour toute la
 *     RAM. Pas de MMU à l'entrée (mode Bare), kernel_main peut activer
 *     Sv39/Sv48 via arch_memory_ops.map().
 *
 * Mémoire (QEMU virt RISC-V) :
 *   RAM : 0x80200000 .. 0x88200000 (128 MiB, à l'exclusion de 0x80000000..0x80200000
 *          réservé à OpenSBI). Le vrai DRAM QEMU commence à 0x80000000, mais on
 *          lie le kernel à 0x80200000 pour ne pas écraser OpenSBI.
 *
 * Symboles exportés (consommés par crt0.c / vector_table.S) :
 *   _sdata     : runtime start de .data (en RAM)
 *   _edata     : runtime end   de .data
 *   _sbss      : start de .bss
 *   _ebss      : end   de .bss
 *   _stack_top : top de la pile noyau (chargé par riscv_reset via `la sp, _stack_top`)
 *   __global_pointer$ : pointeur GP pour la small-data relaxation (charger par
 *                       `la gp, __global_pointer$` dans riscv_reset).
 *
 * NB : pas de _sidata ici — .data est déjà en RAM (pas de flash→RAM copy).
 *      crt0.c l'omet explicitement.
 *
 * Différences avec linker-mcu.ld :
 *   - Pas de FLASH : tout est en RAM (XIP impossible, OpenSBI charge l'ELF).
 *   - Entry = riscv_reset (pas Reset_Handler, pas de vector table au début
 *     de la flash pointée par le hardware : c'est le CSR stvec qui pointe
 *     vers la trap table, installé par crt0_init).
 *   - .trap_vector en section dédiée (alignée 4-byte, KEEP) — crt0_init lit
 *     son adresse pour initialiser stvec.
 */

ENTRY(riscv_reset)

MEMORY
{
    /* QEMU virt RISC-V : 128 MiB de RAM après OpenSBI.
     * Surchargeable via -DRAM_ORIGIN=... -DRAM_LENGTH=... si on cible une
     * vraie carte RISC-V (SiFive U, VisionFive, …) avec un layout différent. */
    RAM (rwx) : ORIGIN = 0x80200000, LENGTH = 128M
}

/* Ajuste stack size : 16 KiB par défaut (kernel C avec recursion limitée),
 * surchargeable via -D__stack_size=0x4000. */
__stack_size = DEFINED(__stack_size) ? __stack_size : 0x4000;

SECTIONS
{
    /* 1. .text — code + read-only data. riscv_reset en premier (entry point). */
    .text : {
        . = ALIGN(4);
        KEEP(*(.text.riscv_reset))      /* entry point — doit être à 0x80200000 */
        *(.text.riscv_default_handler)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > RAM

    /* 2. .trap_vector — table des vecteurs de trap. crt0_init lit son adresse
     *    pour initialiser stvec. Doit être alignée 4-byte et KEEP pour ne pas
     *    être GC'd par --gc-sections (aucun symbole C ne la référence tant que
     *    crt0.c n'a pas tourné). */
    .trap_vector : {
        . = ALIGN(4);
        KEEP(*(.trap_vector))
        . = ALIGN(4);
    } > RAM

    /* 3. .data — runtime en RAM. Pas de LOADADDR distincte (pas de flash) :
     *    .data est déjà chargée par OpenSBI quand il charge l'ELF. */
    .data : {
        . = ALIGN(8);
        _sdata = .;
        *(.data*)
        . = ALIGN(8);
        _edata = .;
    } > RAM

    /* 4. .sdata — small data (accessed via gp-relative, -msmall-data).
     *    Placée dans .data logiquement, mais le linker génère le symbole
     *    __global_pointer$ pour la relaxation. */
    .sdata : {
        . = ALIGN(8);
        __global_pointer$ = . + 0x800;
        *(.srodata*)
        *(.sdata*)
        *(.sbss*)          /* small BSS — zéro-init comme .bss */
        . = ALIGN(8);
    } > RAM

    /* 5. .bss — zero-init (crt0_init la met à zéro). */
    .bss (NOLOAD) : {
        . = ALIGN(8);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > RAM

    /* 6. Pile noyau — placée en haut de la RAM. _stack_top est lu par
     *    riscv_reset via `la sp, _stack_top`. */
    .stack (NOLOAD) : {
        . = ALIGN(16);
        _stack_bottom = .;
        . = . + __stack_size;
        . = ALIGN(16);
        _stack_top = .;
    } > RAM

    /* 7. Symbole de fin de RAM utile pour le heap éventuel. */
    _end = .;
    PROVIDE(end = .);

    /* 8. Sections à ignorer (DWARF, comments, …) — pas gardées dans le binaire
     *    final mais présentes dans l'ELF pour le debug. */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
