/* Linker Script for AfriOS Core x86_64 Bare-Metal */
ENTRY(_start)

MEMORY
{
    /* Physical memory identity-mapped for bare-metal kernel */
    RAM (rwx) : ORIGIN = 0x00100000, LENGTH = 128M /* Kernel loaded at 1MB */
}

SECTIONS
{
    /* Boot and code segment */
    .text.boot : {
        *(.text.boot)
        *(.text)
        *(.text.*)
    } > RAM

    /* Read-only data segment */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > RAM

    /* Initialized data segment - load address = runtime address */
    .data : {
        _data_start = .;
        _data_load_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM

    /* Uninitialized data segment (BSS) */
    .bss : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /* Kernel stack - 16 KB */
    .stack (NOLOAD) : {
        . = ALIGN(16);
        _kernel_stack = .;
        . = . + 0x4000;
        _kernel_stack_top = .;
    } > RAM

    /* End symbols */
    _end = .;
}
