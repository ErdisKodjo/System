# Port RISC-V (RV64GC S-mode : OpenSBI en M-mode, PLIC+CLINT, SBI console,
# virtio-blk, cible QEMU virt).
#
# Cible construite quand -DAFROS_PORT=riscv.
#
# T1/T2 (freestanding-capable) : ce port est désormais bare-metal complet,
# comme port-mcu :
#   - vector_table.S  : table des vecteurs de trap S-mode + riscv_reset
#   - crt0.c          : zero .bss, installe stvec, appelle kernel_main
#   - linker-riscv.ld : layout RAM @ 0x80200000 (Kernel/hal/scripts/linker-riscv.ld)
# Le port reste compatible hosted (build Linux RV64 pour tests), mais le
# firmware bare-metal S-mode est assemblé quand -DAFROS_FREESTANDING=ON.

add_library(afros-port STATIC
    src/cpu_port.c
    src/memory_port.c
    src/interrupt_port.c
    src/timer_port.c
    src/console_port.c
    src/storage_port.c
    src/crt0.c
)

# vector_table.S doit être assemblé en code RISC-V (S-mode, RV64GC). Le
# mode .option norvc dans le fichier impose des entrées 4-byte ; l'assembleur
# RISC-V (riscv64-linux-gnu-as) est requis. enable_language(ASM) active la
# détection de l'assembleur par CMake (il choisira riscv64-linux-gnu-as si
# le toolchain-file toolchain-riscv.cmake est actif, sinon l'`as` du système).
enable_language(ASM)

set(afros_port_asm_sources
    src/vector_table.S
)

add_library(afros-port-asm OBJECT ${afros_port_asm_sources})

target_include_directories(afros-port PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/include
)

# Lien du script linker pour le port RISC-V — les builds exécutables
# bare-metal (firmware .elf) l'utilisent via TARGET_LINK_OPTIONS ... -T
# linker-riscv.ld. Le linker script définit l'entry point riscv_reset et
# place .text à 0x80200000 (convention QEMU virt RISC-V).
set(LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/scripts/linker-riscv.ld
    CACHE FILEPATH "Linker script RISC-V (QEMU virt) pour afros-port"
)
