# CMakeLists.txt for afros-network
#
# Consommateur de afros-hal : on ne référence plus les en-têtes HAL par
# chemin relatif fragile (../afros-core/...) mais via la cible CMake
# `afros-hal` elle-même, qui publie son répertoire d'en-têtes via
# target_include_directories(... PUBLIC include). Si afros-hal n'est pas
# construit (build partiel), on repli sur le chemin absolu basé sur
# CMAKE_SOURCE_DIR.

set(AFROS_NETWORK_SOURCES
    src/afros_net.c
)

add_library(afros-network STATIC ${AFROS_NETWORK_SOURCES})

target_include_directories(afros-network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Préférer la cible afros-hal qui propage transitivement ses includes PUBLIC.
# Repli : pointer vers le répertoire absolu (survit aux déplacements de
# répertoires tant que la racine CMake ne bouge pas).
if(TARGET afros-hal)
    target_link_libraries(afros-network PUBLIC afros-hal)
else()
    target_include_directories(afros-network PUBLIC
        ${CMAKE_SOURCE_DIR}/afros-core/Kernel/hal/include
    )
endif()
