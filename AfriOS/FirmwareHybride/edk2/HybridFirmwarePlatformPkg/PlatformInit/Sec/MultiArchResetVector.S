/*++
  Multi-Architecture Reset Vector for Hybrid Firmware.
  Supports x86_64, AARCH64 and RISC-V (RV64).

  Copyright (c) 2026, AfriOS. All rights reserved.
--*/

#if defined(__x86_64__) || defined(_M_X64)

.text
.align 16
.global _ModuleEntryPoint
_ModuleEntryPoint:
    /* x64 Reset Vector Logic */
    cli
    /* Transition to Long Mode would go here */
    /* ... */
    jmp .

#elif defined(__aarch64__) || defined(_M_ARM64)

.text
.align 16
.global _ModuleEntryPoint
_ModuleEntryPoint:
    /* AARCH64 Reset Vector Logic (EL3) */

    /* 1. Disable interrupts */
    msr daifset, #0xf

    /* 2. Setup Stacks */
    /* ... */

    /* 3. Transition to PEI Phase */
    /* ... */
    b .

#elif defined(__riscv) && (__riscv_xlen == 64)

.text
.align 4
.global _ModuleEntryPoint
_ModuleEntryPoint:
    /* RISC-V (RV64) Reset Vector Logic, cœur démarre en mode M (machine) */

    /* 1. Masquer les interruptions globales (mstatus.MIE) */
    csrci mstatus, 0x8

    /* 2. Initialiser la pile (mscratch/sp) */
    /* ... */

    /* 3. Transition vers la phase PEI */
    /* ... */
    j .

#else
#error "Unsupported Architecture"
#endif
