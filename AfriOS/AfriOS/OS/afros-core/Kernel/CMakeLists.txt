# CMakeLists.txt for afros-core/Kernel
#
# Agrège les couches séparées du noyau AfriOS, dans l'ordre de dépendance
# strict : hal (contrats matériels) -> ports/port-<arch> (implémentation
# matérielle du port sélectionné) -> drivers (périphériques, dépend de hal)
# -> afros (logique noyau générique, dépend de hal + du port). Voir
# hal/docs/architecture.md et ports/README.md pour la justification.

set(AFROS_PORT "arm64" CACHE STRING "Port matériel actif (arm64|x86_64|riscv|mcu)")
set_property(CACHE AFROS_PORT PROPERTY STRINGS arm64 x86_64 riscv mcu)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ports/port-${AFROS_PORT}/CMakeLists.txt")
    message(FATAL_ERROR "AFROS_PORT='${AFROS_PORT}' inconnu — ports disponibles : arm64, x86_64, riscv, mcu (voir Kernel/ports/)")
endif()

add_subdirectory(hal)
add_subdirectory(ports/port-${AFROS_PORT})
add_subdirectory(drivers)
add_subdirectory(afros)

# Étape 5 (Vérification) : tests unitaires HAL. Exclus pour AFROS_PORT=mcu,
# même raison que afros-kernel-sim — voir Kernel/afros/CMakeLists.txt.
if(AFROS_PORT STREQUAL "mcu")
    message(STATUS "AFROS_PORT=mcu : hal/tests non construits (bare-metal, pas de runtime hébergé).")
elseif(AFRIOS_BUILD_TESTS)
    add_subdirectory(hal/tests)
endif()
