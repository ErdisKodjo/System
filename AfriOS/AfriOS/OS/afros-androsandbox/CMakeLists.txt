# CMakeLists.txt for afros-androsandbox

set(ANDROID_SANDBOX_SOURCES
    binder/binder_driver.c
    binder/service_manager.cpp
)

add_library(afros-androsandbox STATIC ${ANDROID_SANDBOX_SOURCES})

target_include_directories(afros-androsandbox PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
