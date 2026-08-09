/*++
  Boot Assembly Entry Point for AfriOS x86_64
  
  Copyright (c) 2026, AfriOS. All rights reserved.
  
  This is the first code executed when the kernel is loaded by the firmware.
  It performs early initialization before jumping to kernel_main in C.
--*/

.code32
.section .text.boot

.global _start
.type _start, @function

_start:
    /* 
     * Entry point from bootloader/firmware
     * At this point:
     * - We are in 32-bit protected mode
     * - Paging is disabled
     * - Stack is not set up
     */

    /* Disable interrupts during early init */
    cli

    /* Check CPU support for long mode */
    mov $0x80000001, %eax
    cpuid
    test $0x20000000, %edx
    jz no_long_mode

    /* Check if PAE is supported */
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    /* Load page table base address (PML4) */
    lea page_table_pml4, %eax
    mov %eax, %cr3

    /* Enable EFER (Extended Feature Enable Register) */
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax        /* Set LME bit (Long Mode Enable) */
    wrmsr

    /* Enable paging to enter long mode */
    mov %cr0, %eax
    or $0x80000000, %eax   /* Set PG bit */
    mov %eax, %cr0

    /* Far jump to 64-bit code segment */
    lgdt gdt64_pointer
    ljmpl $0x08, $long_mode_entry

no_long_mode:
    /* Halt if long mode is not supported */
    hlt
    jmp .

.align 16

.section .data

/* GDT for 64-bit mode */
gdt64:
    .quad 0x0000000000000000    /* Null descriptor */
    .quad 0x00209A0000000000    /* Code segment, base=0, limit=0xFFFFFFFF */
    .quad 0x0000920000000000    /* Data segment */

gdt64_pointer:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

gdt64_end:

/* Page tables for identity mapping (first 2MB) */
.section .bss
.align 4096

page_table_pml4:
    .space 4096

page_table_pdpt:
    .space 4096

page_table_pd:
    .space 4096

.section .text.boot

.code64
.global long_mode_entry
.type long_mode_entry, @function

long_mode_entry:
    /* 
     * Now in 64-bit long mode
     * Set up stack and call kernel_main
     */

    /* Clear segment registers */
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Set up stack */
    lea _kernel_stack_top(%rip), %rsp

    /* Zero out BSS section */
    lea _bss_start(%rip), %rdi
    lea _bss_end(%rip), %rcx
    sub %rdi, %rcx
    xor %eax, %eax
    rep stosb

    /* Copy data section from ROM to RAM (if needed) */
    lea _data_start(%rip), %rsi
    lea _data_load_start(%rip), %rdi
    lea _data_end(%rip), %rcx
    sub %rdi, %rcx
    rep movsb

    /* Call kernel main entry point */
    call kernel_main

    /* Kernel should never return, but just in case */
    cli
    hlt
    jmp .

/* Stack definition */
.section .bss
.align 16

_kernel_stack:
    .space 0x4000  /* 16 KB stack */
_kernel_stack_top:
