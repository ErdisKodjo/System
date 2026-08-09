# Port ARM64 (ARMv8/v9, GICv3, Generic Timer, PL011, eMMC/UFS).
#
# Cible construite quand -DAFROS_PORT=arm64 (défaut). Fournit les tables
# d'opérations déclarées dans hal/include/*_abstraction.h : c'est le SEUL
# endroit où l'architecture ARM64 apparaît dans le noyau AfriOS.

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
