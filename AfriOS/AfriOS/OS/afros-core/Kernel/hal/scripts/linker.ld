/* Script de liaison (Linker Script) pour AfriOS Core ARM64 */
ENTRY(kernel_main)

MEMORY
{
    /* Définition des segments de mémoire */
    FLASH (rx) : ORIGIN = 0x40000000, LENGTH = 2M   /* Zone du noyau */
    RAM (rwx)  : ORIGIN = 0x80000000, LENGTH = 128M /* Zone de données */
}

SECTIONS
{
    /* Segment de code (Text) */
    .text : {
        KEEP(*( .vector_table ))
        *(.text)
        *(.text.*)
    } > FLASH

    /* Segment de données constantes (Read-only) */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > FLASH

    /* Segment de données initialisées (Data) */
    .data : {
        *(.data)
        *(.data.*)
    } > RAM AT > FLASH

    /* Segment de données non initialisées (BSS) */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > RAM

    /* Pile du noyau (Stack) */
    .stack (NOLOAD) : {
        . = ALIGN(16);
        . = . + 0x4000; /* Pile de 16 KB */
        _kernel_stack_top = .;
    } > RAM
}
