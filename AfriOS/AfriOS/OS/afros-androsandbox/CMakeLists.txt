# CMakeLists.txt for afros-androsandbox
# =============================================================================
# afros-androsandbox is the AfriOS Android-side compatibility runtime: it
# exposes a Binder driver, an Android-style VFS (sdcard, ashmem, properties,
# SELinux emulation) and the higher-level Android runtime stack (dalvikvm,
# ART, dex2oat, the optimizing compiler, SurfaceFlinger, the framework
# services and the system services). It is shipped as a single shared
# library, libafros-androsandbox.so, that afros-corebridge loads when an
# Android APK is the selected runtime target.
#
# The C side (binder/, vfs/) is plain C11; the runtime side (dalvikvm/,
# art/runtime/, dex2oat/, compiler/, surfaceflinger/, framework/*/,
# services/) is C++17. binder/ mixes both languages: its IPC marshalling
# code is .cpp (uses STL containers) while the in-kernel driver shim is .c.
# =============================================================================

set(AFROS_ANDROSANDBOX_SOURCES
    # --- Binder IPC (C driver shim + C++ userland) -------------------------
    binder/binder_driver.c
    binder/parcel.cpp
    binder/transaction.cpp
    binder/reference_tracker.cpp
    binder/service_manager.cpp

    # --- Android-style VFS --------------------------------------------------
    vfs/android_fs.c
    vfs/ashmem.c
    vfs/properties.c
    vfs/sdcard_emulation.c
    vfs/selinux_emulation.c

    # --- Dalvik VM ----------------------------------------------------------
    dalvikvm/dalvikvm.cc

    # --- ART runtime --------------------------------------------------------
    art/runtime/art_runtime.cc
    art/runtime/class_linker.cc
    art/runtime/jit_comlier.cc

    # --- dex2oat (ahead-of-time compiler driver) ---------------------------
    dex2oat/dex2oat_main.cc
    dex2oat/compiler_driver.cc

    # --- Optimizing compiler ------------------------------------------------
    compiler/optimizing_compiler.cc
    compiler/dex_to_native.cc

    # --- SurfaceFlinger & HWComposer ---------------------------------------
    surfaceflinger/surface_flinger.cpp
    surfaceflinger/buffer_queue.cpp
    surfaceflinger/layer.cpp
    surfaceflinger/hwcomposer_hal.cpp

    # --- Framework services -------------------------------------------------
    framework/activity_manager/activity_manager_service.cpp
    framework/activity_manager/activity_stack.cpp
    framework/activity_manager/task_record.cpp
    framework/window_manager/window_manager_service.cpp
    framework/window_manager/display_content.cpp
    framework/window_manager/window_state.cpp
    framework/package_manager/package_manager_service.cpp
    framework/package_manager/package_parser.cpp
    framework/package_manager/permission_manager.cpp
    framework/content/content_resolver.cpp
    framework/content/content_provider.cpp
    framework/content/uri_matcher.cpp
    framework/telephony/telephony_manager.cpp
    framework/telephony/phone_interface.cpp

    # --- System services ----------------------------------------------------
    services/audio_service.cpp
    services/camera_service.cpp
    services/location_service.cpp
    services/notification_service.cpp
    services/sensor_service.cpp
)

add_library(afros-androsandbox SHARED ${AFROS_ANDROSANDBOX_SOURCES})

set_target_properties(afros-androsandbox PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "afros-androsandbox"
)

target_include_directories(afros-androsandbox PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -lpthread: Binder transactions, SurfaceFlinger composer thread and the ART
# JIT compiler all spawn background threads.
if(NOT TARGET Threads::Threads)
    find_package(Threads REQUIRED)
endif()
target_link_libraries(afros-androsandbox PUBLIC Threads::Threads)

# --- Install rules -----------------------------------------------------------
install(TARGETS afros-androsandbox
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
                                    PATTERN "*.hpp")
