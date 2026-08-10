# CMakeLists.txt for afros-power-management
#
# Consommateur de afros-hal : on ne référence plus les en-têtes HAL par
# chemin relatif fragile (../afros-core/...) mais via la cible CMake
# `afros-hal` elle-même, qui publie son répertoire d'en-têtes via
# target_include_directories(... PUBLIC include). Si afros-hal n'est pas
# construit (build partiel), on repli sur le chemin absolu basé sur
# CMAKE_SOURCE_DIR.

set(AFROS_POWER_SOURCES
    src/battery_monitor.c
)

add_library(afros-power-management STATIC ${AFROS_POWER_SOURCES})

target_include_directories(afros-power-management PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# afros_hal_ops (extern global) est défini dans afros-hal/src/hal_init.c :
# ce service doit être lié à la lib, pas seulement inclure ses en-têtes.
if(TARGET afros-hal)
    target_link_libraries(afros-power-management PUBLIC afros-hal)
else()
    target_include_directories(afros-power-management PUBLIC
        ${CMAKE_SOURCE_DIR}/afros-core/Kernel/hal/include
    )
endif()
