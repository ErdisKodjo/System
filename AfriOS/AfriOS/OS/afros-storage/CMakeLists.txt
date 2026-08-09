# CMakeLists.txt for afros-storage

set(AFROS_STORAGE_SOURCES
    src/storage_mgr.c
)

add_library(afros-storage STATIC ${AFROS_STORAGE_SOURCES})

target_include_directories(afros-storage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)

if(TARGET afros-hal)
    target_link_libraries(afros-storage PUBLIC afros-hal)
endif()
