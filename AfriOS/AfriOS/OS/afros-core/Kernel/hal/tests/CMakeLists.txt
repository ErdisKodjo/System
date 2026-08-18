# Tests unitaires HAL (étape 5 + étape P1). Non construits pour
# AFROS_PORT=mcu — même raison que afros-kernel-sim (Kernel/afros/CMakeLists.txt)
# et afros-hal-tests : bare-metal, pas de runtime hébergé pour exécuter un
# binaire de test classique.
#
# Étape P1 : ajout de `hal_test_runner.c` — runner unifié qui couvre tous les
# cas `[ ]` de tests.md plus les cas génériques (HAL init/deinit, CPU/memory/
# timer/console/storage/device ops). Lié à afros-hal + afros-port, tourne
# sur host en mode libc stdio.

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
if(AFROS_HAL_TEST_HOST_MOCK)
    # Override the afros-port link from above: when host-mock is on, the
    # test runner links against the host-mock OBJECT library instead of the
    # bare-metal afros-port. We use PRIVATE so the bare-metal afros-port
    # (if it was already linked) is shadowed by the host-mock definitions
    # of arch_console_ops / arch_cpu_ops / etc.
    target_link_libraries(hal_test_runner PRIVATE afros-port-host-mock afros-hal)
    target_compile_definitions(hal_test_runner PRIVATE AFROS_HOST_MOCK=1)
    # Same for the smoke test so both stay consistent.
    target_link_libraries(afros-hal-tests PRIVATE afros-port-host-mock afros-hal)
    target_compile_definitions(afros-hal-tests PRIVATE AFROS_HOST_MOCK=1)
endif()
