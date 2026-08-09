# CMakeLists.txt for afros-corebridge-core

set(COREBRIDGE_SOURCES
    src/central_manager.c
    src/runtime_registry.c
)

add_library(afros-corebridge-core STATIC ${COREBRIDGE_SOURCES})

target_include_directories(afros-corebridge-core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
