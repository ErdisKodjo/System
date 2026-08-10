# Port MCU (Cortex-M générique : mono-cœur, NVIC, SysTick, USART, SPI NOR).
#
# Cible construite quand -DAFROS_PORT=mcu. Plusieurs opérations renvoient
# AFROS_ERROR_NOT_SUPPORTED (migrate_task, send_ipi, memory map/compress,
# storage write) car elles n'ont pas de sens sans MMU ni second cœur — ce
# n'est pas un manque d'implémentation mais une contrainte matérielle réelle.
#
# T1/T2 (freestanding) : ce port est désormais bare-metal complet :
#   - vector_table.S  : table des vecteurs Cortex-M + Reset_Handler
#   - crt0.c          : copie .data, zéro .bss, appelle kernel_main
#   - linker-mcu.ld   : layout flash/ram (Kernel/hal/scripts/linker-mcu.ld)
# Les autres ports (arm64/riscv/x86_64) restent en build hébergée/simulée.

add_library(afros-port STATIC
    src/cpu_port.c
    src/memory_port.c
    src/interrupt_port.c
    src/timer_port.c
    src/console_port.c
    src/storage_port.c
    src/crt0.c
)

# vector_table.S doit être assemblé en Thumb (le hardware Cortex-M attend des
# adresses de handler avec bit 0 = 1). enable_language(ASM) est requis en haut
# du CMake racine — y est par défaut (LANGUAGES C CXX étendu par ASM via
# project() si ASM est listé ; sinon l'ajouter via enable_language(ASM) ici).
enable_language(ASM)

set(afros_port_asm_sources
    src/vector_table.S
)

add_library(afros-port-asm OBJECT ${afros_port_asm_sources})

target_include_directories(afros-port PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/include
)

# Lien du script linker pour le port MCU — les builds exécutables bare-metal
# (firmware .elf) l'utilisent via TARGET_LINK_OPTIONS ... -T linker-mcu.ld.
set(LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/scripts/linker-mcu.ld
    CACHE FILEPATH "Linker script Cortex-M pour afros-port"
)
