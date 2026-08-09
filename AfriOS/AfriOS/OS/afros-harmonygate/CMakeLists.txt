# CMakeLists.txt for afros-harmonygate

set(HARMONY_COMPAT_SOURCES
    ability/ability_runtime/ability_manager.cpp
)

add_library(afros-harmonygate STATIC ${HARMONY_COMPAT_SOURCES})

target_include_directories(afros-harmonygate PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
