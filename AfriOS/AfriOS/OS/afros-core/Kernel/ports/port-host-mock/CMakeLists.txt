# Port host-mock — Linux userspace stubs for the HAL.
#
# Cible construite quand on veut exécuter le HAL test runner sur une machine
# Linux standard (CI runner) sans bare-metal ni QEMU. Fournit les mêmes
# arch_*_ops que port-x86_64 / port-arm64 / port-riscv / port-mcu mais avec
# des implémentations userspace-safe (libc + POSIX) : printf pour la console,
# malloc/mmap pour la mémoire, clock_gettime pour le timer, fopen pour le
# stockage, etc.
#
# Active via l'option CMake AFROS_HAL_TEST_HOST_MOCK (voir
# Kernel/hal/tests/CMakeLists.txt) ou par appel gcc direct
# (voir scripts/run-hal-tests.sh).
#
# NB: cette cible ne définit PAS AFROS_FREESTANDING — au contraire, elle
# suppose le runtime libc complet (exit, fprintf, main() qui retourne à
# l'OS).  C'est intentional : le host-mock est le pendant hébergé des ports
# bare-metal.

add_library(afros-port-host-mock OBJECT
    src/console_port.c
    src/cpu_port.c
    src/memory_port.c
    src/interrupt_port.c
    src/timer_port.c
    src/storage_port.c
)

target_include_directories(afros-port-host-mock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Marqueur de compilation pour que les tests puissent activer des branches
# host-mock-only (symétrique de AFROS_FREESTANDING pour les builds hébergés).
target_compile_definitions(afros-port-host-mock PUBLIC AFROS_HOST_MOCK=1)

# Sur Linux, on a besoin de pthread pour timer_create(SIGEV_THREAD) et de rt
# pour clock_gettime / timer_create (bien que glibc récent les ait intégrés
# dans libc, le symbole -lrt reste la forme portable).
target_link_libraries(afros-port-host-mock PUBLIC m rt pthread)

# Le host-mock ne doit JAMAIS être compilé en mode freestanding : il repose
# sur la libc et sur main() qui retourne à l'OS.
if(AFROS_FREESTANDING)
    message(WARNING
        "afros-port-host-mock is being built with AFROS_FREESTANDING=1. "
        "The host-mock port requires the hosted libc runtime — "
        "AFROS_FREESTANDING will be undefined for this target only.")
    target_compile_definitions(afros-port-host-mock PRIVATE
        $<$<BOOL:AFROS_FREESTANDING>:UNDEF_AFROS_FREESTANDING_FOR_HOST_MOCK>)
endif()
