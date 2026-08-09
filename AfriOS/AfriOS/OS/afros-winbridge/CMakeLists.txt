# CMakeLists.txt for afros-winbridge

set(AFROS_WINBRIDGE_SOURCES
    wine/loader/wine_loader.c
)

add_library(afros-winbridge STATIC ${AFROS_WINBRIDGE_SOURCES})

target_include_directories(afros-winbridge PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/wine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
