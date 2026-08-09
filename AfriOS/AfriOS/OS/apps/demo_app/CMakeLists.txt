# CMakeLists.txt for demo_app

add_executable(demo_app main.c)

target_link_libraries(demo_app PRIVATE 
    afros-sdk 
    afros-network 
    afros-storage 
    afros-power-management
)

# HAL headers needed for some libraries
target_include_directories(demo_app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../afros-core/Kernel/hal/include
)
