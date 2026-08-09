# CMakeLists.txt for afros-infrastructure

set(INFRA_SOURCES
    src/infra_core.c
)

add_library(afros-infrastructure STATIC ${INFRA_SOURCES})

target_include_directories(afros-infrastructure PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
