# Port RISC-V (RV64GC : harts, PLIC+CLINT, SBI console, virtio-blk).
#
# Cible construite quand -DAFROS_PORT=riscv.

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
