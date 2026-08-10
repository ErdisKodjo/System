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
