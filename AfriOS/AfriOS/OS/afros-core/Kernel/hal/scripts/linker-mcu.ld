/*
 * linker-mcu.ld — Linker script pour le port MCU AfriOS (ARM Cortex-M).
 *
 * Conventions Cortex-M :
 *   - FLASH @ 0x08000000 (mapping alias du boot, XIP sur STM32, NXP, …).
 *   - RAM   @ 0x20000000 (SRAM interne, convention ARMv7-M).
 *   - Vecteur d'interruption en tête de flash : le hardware charge SP depuis
 *     g_pfnVectors[0] et PC depuis g_pfnVectors[1] au reset.
 *
 * Symboles exportés (consommés par crt0.c) :
 *   _sidata   : load address de .data (en flash)
 *   _sdata    : runtime start de .data (en RAM)
 *   _edata    : runtime end   de .data (en RAM)
 *   _sbss     : start de .bss (en RAM)
 *   _ebss     : end   de .bss (en RAM)
 *   _stack_top: top de la pile noyau (init SP, chargé depuis g_pfnVectors[0])
 *
 * Ces valeurs sont ajustables via -DFLASH_ORIGIN=... -DRAM_ORIGIN=...
 * -DFLASH_LENGTH=... -DRAM_LENGTH=... à la configuration du build.
 *
 * NB : le linker.ld historique (Kernel/hal/scripts/linker.ld) reste valable
 * pour le port x86_64 (RAM identity-mapped à 1 MiB). Ce script distinct évite
 * de casser ce dernier tout en supportant le layout Cortex-M.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Ajuste stack size : 4 KiB par défaut, surchargeable. */
__stack_size = DEFINED(__stack_size) ? __stack_size : 0x1000;

SECTIONS
{
    /* 1. Vector table — obligatoirement en tête de flash. */
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* 2. Code + read-only data. */
    .text : {
        . = ALIGN(4);
        *(.text.Reset_Handler)        /* reset handler en premier après vecteurs */
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* 3. .data — runtime en RAM, load address en flash (AT>FLASH). */
    .data : {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);

    /* 4. .bss — zero-init (crt0_init la met à zéro). */
    .bss (NOLOAD) : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* 5. Pile noyau — placée en haut de la RAM. _stack_top est lu par le
     *    hardware depuis g_pfnVectors[0] au reset. */
    .stack (NOLOAD) : {
        . = ALIGN(8);
        _stack_bottom = .;
        . = . + __stack_size;
        . = ALIGN(8);
        _stack_top = .;
    } > RAM

    /* 6. Symbole de fin de RAM utile pour le heap éventuel. */
    _end = .;
    PROVIDE(end = .);
}
