# Port x86_64 (Local APIC/IOAPIC, TSC-deadline, UART 16550, NVMe).
#
# Cible construite quand -DAFROS_PORT=x86_64.

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
