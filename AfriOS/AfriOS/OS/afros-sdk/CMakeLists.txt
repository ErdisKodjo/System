# CMakeLists.txt for afros-sdk

set(AFROS_SDK_SOURCES
    src/afros_sdk.c
)

add_library(afros-sdk STATIC ${AFROS_SDK_SOURCES})

target_include_directories(afros-sdk PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
