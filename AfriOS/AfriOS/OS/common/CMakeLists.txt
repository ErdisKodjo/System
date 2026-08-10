# CMakeLists.txt for common

set(COMMON_SOURCES
    src/utils.c
)

add_library(afros-common STATIC ${COMMON_SOURCES})

target_include_directories(afros-common PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
