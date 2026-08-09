# CMakeLists.txt for afros-power-management

set(AFROS_POWER_SOURCES
    src/battery_monitor.c
)

add_library(afros-power-management STATIC ${AFROS_POWER_SOURCES})

target_include_directories(afros-power-management PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../afros-core/Kernel/hal/include
)

# afros_hal_ops (extern global) est défini dans afros-hal/src/hal_init.c :
# ce service doit être lié à la lib, pas seulement inclure ses en-têtes.
if(TARGET afros-hal)
    target_link_libraries(afros-power-management PUBLIC afros-hal)
endif()
