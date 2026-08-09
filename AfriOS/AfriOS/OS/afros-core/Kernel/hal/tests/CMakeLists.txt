# Tests unitaires HAL (étape 5). Non construits pour AFROS_PORT=mcu — même
# raison que afros-kernel-sim (Kernel/afros/CMakeLists.txt) : bare-metal,
# pas de runtime hébergé pour exécuter un binaire de test classique.

add_executable(afros-hal-tests hal_smoke_test.c)
target_link_libraries(afros-hal-tests PRIVATE afros-hal afros-port)

add_test(NAME afros-hal-smoke COMMAND afros-hal-tests)
