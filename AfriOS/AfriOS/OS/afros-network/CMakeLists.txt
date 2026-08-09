# CMakeLists.txt for afros-network

set(AFROS_NETWORK_SOURCES
    src/afros_net.c
)

add_library(afros-network STATIC ${AFROS_NETWORK_SOURCES})

target_include_directories(afros-network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)

if(TARGET afros-hal)
    target_link_libraries(afros-network PUBLIC afros-hal)
endif()
