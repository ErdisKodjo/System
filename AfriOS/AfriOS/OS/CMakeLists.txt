# AfriOS Root CMakeLists.txt
# Main build file for the entire project

cmake_minimum_required(VERSION 3.16)

project(AfriOS
    DESCRIPTION "Multi-platform OS with unified runtime"
    LANGUAGES C CXX ASM
)

set(AFROS_VERSION "1.0.0")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options — la forme canonique est AFROS_* (cohérente avec afros-core/ qui
# utilise déjà AFROS_PORT etc.). L'ancien préfixe AFRIOS_* reste accepté pour
# rétro-compatibilité (voir bloc d'alias ci-dessous).
option(AFROS_BUILD_ANDROID  "Build Android framework"     ON)
option(AFROS_BUILD_KERNEL   "Build AfriOS kernel"         ON)
option(AFROS_BUILD_RUNTIME  "Build runtime manager"       ON)
option(AFROS_BUILD_GRAPHICS "Build graphics subsystem"    ON)
option(AFROS_BUILD_TESTS    "Build test suite"            ON)
option(AFROS_ENABLE_LTO     "Enable Link Time Optimization" OFF)
option(AFROS_FREESTANDING   "Build the kernel freestanding (no libc)" OFF)

# Backward-compat : si un utilisateur/CI passe encore l'ancien nom
# -D<AFRIOS_*>=... sans le nouveau AFROS_*, on le propage. Une fois l'option
# AFROS_* explicitement positionnée, l'alias est ignoré.
foreach(_opt
        BUILD_ANDROID BUILD_KERNEL BUILD_RUNTIME BUILD_GRAPHICS BUILD_TESTS
        ENABLE_LTO)
    if(DEFINED AFRIOS_${_opt} AND NOT DEFINED AFROS_${_opt})
        set(AFROS_${_opt} ${AFRIOS_${_opt}})
    endif()
endforeach()

# Compiler flags
if(AFROS_ENABLE_LTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -flto")
endif()

# Enable testing
enable_testing()

# Add subdirectories
if(AFROS_BUILD_KERNEL)
    add_subdirectory(afros-core/Kernel)
endif()

if(AFROS_BUILD_ANDROID)
    add_subdirectory(afros-androsandbox)
endif()

if(AFROS_BUILD_RUNTIME)
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

if(AFROS_BUILD_GRAPHICS)
    # add_subdirectory(afros-dxvk)
    message(STATUS "DXVK subproject (commented - requires meson)")
endif()

if(AFROS_BUILD_TESTS)
    add_subdirectory(test)
endif()

# Print summary
message(STATUS "")
message(STATUS "AfriOS ${AFROS_VERSION} Configuration Summary:")
message(STATUS "  Build Android Framework: ${AFROS_BUILD_ANDROID}")
message(STATUS "  Build Kernel:           ${AFROS_BUILD_KERNEL}")
message(STATUS "  Build Runtime Manager:  ${AFROS_BUILD_RUNTIME}")
message(STATUS "  Build Graphics:         ${AFROS_BUILD_GRAPHICS}")
message(STATUS "  Build Tests:            ${AFROS_BUILD_TESTS}")
message(STATUS "  LTO Enabled:            ${AFROS_ENABLE_LTO}")
message(STATUS "  Freestanding kernel:    ${AFROS_FREESTANDING}")
message(STATUS "")
