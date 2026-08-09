# AfriOS Root CMakeLists.txt
# Main build file for the entire project

cmake_minimum_required(VERSION 3.16)

project(AfriOS 
    DESCRIPTION "Multi-platform OS with unified runtime"
    LANGUAGES C CXX
)

set(AFRIOS_VERSION "1.0.0")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(AFRIOS_BUILD_ANDROID "Build Android framework" ON)
option(AFRIOS_BUILD_KERNEL "Build AfriOS kernel" ON)
option(AFRIOS_BUILD_RUNTIME "Build runtime manager" ON)
option(AFRIOS_BUILD_GRAPHICS "Build graphics subsystem" ON)
option(AFRIOS_BUILD_TESTS "Build test suite" ON)
option(AFRIOS_ENABLE_LTO "Enable Link Time Optimization" OFF)

# Compiler flags
if(AFRIOS_ENABLE_LTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
endif()

# Enable testing
enable_testing()

# Add subdirectories
if(AFRIOS_BUILD_KERNEL)
    add_subdirectory(afros-core/Kernel)
endif()

if(AFRIOS_BUILD_ANDROID)
    add_subdirectory(afros-androsandbox)
endif()

if(AFRIOS_BUILD_RUNTIME)
    add_subdirectory(afros-corebridge-core)
endif()

add_subdirectory(afros-network)
add_subdirectory(afros-storage)
add_subdirectory(afros-power-management)
add_subdirectory(afros-winbridge)
add_subdirectory(afros-sdk)
add_subdirectory(afros-package-manager)
add_subdirectory(afros-incompat-engine)
add_subdirectory(afros-harmonygate)
add_subdirectory(afros-babelbridge-core)
add_subdirectory(afros-infrastructure)
add_subdirectory(common)

# Applications
add_subdirectory(apps/demo_app)

if(AFRIOS_BUILD_GRAPHICS)
    # add_subdirectory(afros-dxvk)
    message(STATUS "DXVK subproject (commented - requires meson)")
endif()

if(AFRIOS_BUILD_TESTS)
    add_subdirectory(test)
endif()

# Print summary
message(STATUS "")
message(STATUS "AfriOS ${AFRIOS_VERSION} Configuration Summary:")
message(STATUS "  Build Android Framework: ${AFRIOS_BUILD_ANDROID}")
message(STATUS "  Build Kernel: ${AFRIOS_BUILD_KERNEL}")
message(STATUS "  Build Runtime Manager: ${AFRIOS_BUILD_RUNTIME}")
message(STATUS "  Build Graphics: ${AFRIOS_BUILD_GRAPHICS}")
message(STATUS "  Build Tests: ${AFRIOS_BUILD_TESTS}")
message(STATUS "  LTO Enabled: ${AFRIOS_ENABLE_LTO}")
message(STATUS "")
