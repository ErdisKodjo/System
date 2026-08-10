# CMakeLists.txt for afros-storage
#
# Consommateur de afros-hal : on ne référence plus les en-têtes HAL par
# chemin relatif fragile (../afros-core/...) mais via la cible CMake
# `afros-hal` elle-même, qui publie son répertoire d'en-têtes via
# target_include_directories(... PUBLIC include). Si afros-hal n'est pas
# construit (build partiel), on repli sur le chemin absolu basé sur
# CMAKE_SOURCE_DIR.

set(AFROS_STORAGE_SOURCES
    src/storage_mgr.c
)

add_library(afros-storage STATIC ${AFROS_STORAGE_SOURCES})

target_include_directories(afros-storage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(TARGET afros-hal)
    target_link_libraries(afros-storage PUBLIC afros-hal)
else()
    target_include_directories(afros-storage PUBLIC
        ${CMAKE_SOURCE_DIR}/afros-core/Kernel/hal/include
    )
endif()
