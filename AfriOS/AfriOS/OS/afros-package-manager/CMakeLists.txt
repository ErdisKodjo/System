# CMakeLists.txt for afros-package-manager

set(AFROS_PKG_MGR_SOURCES
    src/pkg_mgr.c
)

add_library(afros-package-manager STATIC ${AFROS_PKG_MGR_SOURCES})

target_include_directories(afros-package-manager PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)
