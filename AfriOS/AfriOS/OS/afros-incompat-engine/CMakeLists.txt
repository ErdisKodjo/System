# CMakeLists.txt for afros-incompat-engine

set(APPLE_COMPAT_SOURCES
    macho_loader/loader.c
)

add_library(afros-incompat-engine STATIC ${APPLE_COMPAT_SOURCES})

target_include_directories(afros-incompat-engine PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
