# Tests unitaires HAL (étape 5 + étape P1). Non construits pour
# AFROS_PORT=mcu — même raison que afros-kernel-sim (Kernel/afros/CMakeLists.txt)
# et afros-hal-tests : bare-metal, pas de runtime hébergé pour exécuter un
# binaire de test classique.
#
# IMPORTANT : les tests HAL ne sont construits QUE pour le port host-mock
# (tests CI sur Linux host). En cross-compilation (arm64, riscv, mcu), les
# tests ne peuvent pas s'exécuter sur le host et le target afros-port-host-mock
# n'existe pas — on skippe donc entièrement.
if(NOT AFROS_PORT STREQUAL "host-mock")
    message(STATUS "AFROS_PORT=${AFROS_PORT} : hal/tests non construits (cross-compile, port-host-mock requis).")
    return()
endif()

# 1. Smoke test original (étape 5) — couverture minimale historique.
add_executable(afros-hal-tests hal_smoke_test.c)
target_link_libraries(afros-hal-tests PRIVATE afros-hal afros-port)

# 2. Runner unifié P1 — couvre tous les cas `[ ]` de tests.md + cas génériques.
add_executable(hal_test_runner hal_test_runner.c)
target_link_libraries(hal_test_runner PRIVATE afros-hal afros-port)

# 3. CTest registration : enable_testing() doit être appelé au plus haut
#    niveau du projet (CMakeLists.txt racine). On l'appelle aussi ici pour
#    permettre l'exécution isolée de ce sous-répertoire (cmake -B build -S
#    afros-core/Kernel/hal/tests).
enable_testing()

add_test(NAME afros-hal-smoke COMMAND afros-hal-tests)
add_test(NAME hal_test COMMAND hal_test_runner)

# 4. Host-mock port (étape AU) — permet au hal_test_runner de s'exécuter
#    sur un CI runner Linux standard, sans bare-metal ni QEMU. Le port
#    host-mock (afros-core/Kernel/ports/port-host-mock/) fournit les mêmes
#    arch_*_ops que port-x86_64 mais avec des implémentations userspace-safe
#    (printf, malloc, mmap, clock_gettime, fopen).
#
# Active par défaut (ON) — désactive avec -DAFROS_HAL_TEST_HOST_MOCK=OFF si
# tu veux vraiment tester contre port-x86_64 (qui requiert les instructions
# privilégiées lgdt/lidt/inb/outb et ne tournera pas sur un Linux host).
option(AFROS_HAL_TEST_HOST_MOCK
       "Build HAL tests with the host-mock port (no bare-metal required)" ON)
if(AFROS_HAL_TEST_HOST_MOCK AND TARGET afros-port-host-mock)
    target_link_libraries(hal_test_runner PRIVATE afros-port-host-mock afros-hal)
    target_compile_definitions(hal_test_runner PRIVATE AFROS_HOST_MOCK=1)
    target_link_libraries(afros-hal-tests PRIVATE afros-port-host-mock afros-hal)
    target_compile_definitions(afros-hal-tests PRIVATE AFROS_HOST_MOCK=1)
endif()
