# CMakeLists.txt for afros-babelbridge-core

set(BABELBRIDGE_SOURCES
    src/babel_core.c
)

add_library(afros-babelbridge-core STATIC ${BABELBRIDGE_SOURCES})

target_include_directories(afros-babelbridge-core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-corebridge-core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
