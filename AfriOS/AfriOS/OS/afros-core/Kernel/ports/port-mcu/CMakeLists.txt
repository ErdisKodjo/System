# Port MCU (Cortex-M générique : mono-cœur, NVIC, SysTick, USART, SPI NOR).
#
# Cible construite quand -DAFROS_PORT=mcu. Plusieurs opérations renvoient
# AFROS_ERROR_NOT_SUPPORTED (migrate_task, send_ipi, memory map/compress,
# storage write) car elles n'ont pas de sens sans MMU ni second cœur — ce
# n'est pas un manque d'implémentation mais une contrainte matérielle réelle.

add_library(afros-port STATIC
    src/cpu_port.c
    src/memory_port.c
    src/interrupt_port.c
    src/timer_port.c
    src/console_port.c
    src/storage_port.c
)

target_include_directories(afros-port PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/include
)
