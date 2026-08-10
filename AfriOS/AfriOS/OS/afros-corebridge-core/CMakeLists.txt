# CMakeLists.txt for afros-corebridge-core
# =============================================================================
# afros-corebridge is the AfriOS "central manager": it detects incoming
# applications (loader/), selects the most appropriate runtime manager
# (runtime_managers/, src/selection_engine.c), exposes a unified execution
# surface (filesystem/address-space/network/resource views) and keeps every
# component versioned & up-to-date (version_management/). It links against the
# afros-hal target so it can request CPU/memory/device resources from the
# kernel HAL when running inside AfriOS, but the same library also links on a
# pure host (Linux) build for use by the runtime-selector and compatibility
# tester tools under tools/.
# =============================================================================

# Sources are listed explicitly (rather than via file(GLOB)) so that adding a
# new translation unit is a deliberate act — these modules are large enough
# that globbing tends to silently pick up half-finished files.
set(AFROS_COREBRIDGE_SOURCES
    # --- Central orchestrator ----------------------------------------------
    src/central_manager.c
    src/runtime_registry.c
    src/selection_engine.c
    src/monitoring.c
    src/resource_allocator.c

    # --- Public API entry points + version string (Tier 1 Stable) ---------
    src/api_version.c

    # --- Application loader (format sniffing + dependency resolution) ------
    loader/app_detector.c
    loader/format_analyzer.c
    loader/intelligent_loader.c
    loader/dependency_resolver.c

    # --- Per-OS runtime managers -------------------------------------------
    runtime_managers/linux_runtime_manager.c
    runtime_managers/win_runtime_manager.c
    runtime_managers/android_runtime_manager.cpp
    runtime_managers/ios_runtime_manager.cpp
    runtime_managers/harmony_runtime_manager.c

    # --- Unified execution surface -----------------------------------------
    unified_execution/filesystem_view.c
    unified_execution/address_space.c
    unified_execution/network_stack.c
    unified_execution/resource_manager.c

    # --- Version management -------------------------------------------------
    version_management/version_registry.c
    version_management/update_checker.c
    version_management/downloader.c
    version_management/installer.c
)

add_library(afros-corebridge STATIC ${AFROS_COREBRIDGE_SOURCES})

# C++17 for the Android/iOS runtime managers (STL-heavy containers).
set_target_properties(afros-corebridge PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Public include directory: consumers see include/ (and the HAL headers
# transitively, because afros-hal already exposes them PUBLIC).
target_include_directories(afros-corebridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Threads::Threads is the portable way to ask for -lpthread. It requires
# FindThreads to have been run; do it lazily here so this file remains
# self-contained when consumed via add_subdirectory().
if(NOT TARGET Threads::Threads)
    find_package(Threads REQUIRED)
endif()

# Link against the HAL target so we can call hal_request_cpu()/etc. when
# running inside AfriOS. -lpthread is needed by src/monitoring.c which spins
# up a background monitoring thread.
target_link_libraries(afros-corebridge PUBLIC
    afros-hal
    Threads::Threads
)

# --- Install rules -----------------------------------------------------------
install(TARGETS afros-corebridge
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Install the entire include/ tree. The umbrella header
# include/afros_corebridge.h is the single header third-party consumers
# should #include to get the full Tier 1 / Tier 2 public surface; the
# per-subsystem headers (loader.h, runtime_manager.h, version_mgmt.h,
# babelbridge.h, orchestrator.h) are installed alongside it.
install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING
            PATTERN "*.h"
            PATTERN "*.hpp")

# Explicit install of the umbrella header is redundant with the rule
# above, but documents that afros_corebridge.h is part of the public
# API contract (frozen as of v1.0.0 — see API.md and CHANGELOG.md).
install(FILES include/afros_corebridge.h
        DESTINATION include)
