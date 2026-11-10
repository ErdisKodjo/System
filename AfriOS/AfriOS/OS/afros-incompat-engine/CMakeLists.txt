cmake_minimum_required(VERSION 3.16)
project(afrosincompatengine LANGUAGES C CXX ASM)

# CMakeLists.txt for afros-incompat-engine
# =============================================================================
# afros-incompat-engine is AfriOS's Apple-platform compatibility layer,
# modelled on Darling (the macOS/iOS-on-Linux project). It is shipped as a
# single shared library, libafros-incompat-engine.so, that bundles:
#
#   * macho_loader/ — Mach-O parser, dyld emulator, symbol & binding
#                     resolvers that turn an Apple binary into something
#                     the AfriOS ELF loader can map.
#   * runtime/      — Objective-C runtime (class_loader, message_dispatch,
#                     objc_runtime) and ARC.
#   * filesystem/   — APFS, HFS+, bundle manager & iCloud stub.
#   * sandbox/      — iOS sandbox, entitlements, data protection, container
#                     manager.
#   * codesign/     — signature verifier, certificate chain, provisioning
#                     profile parser.
#   * darling/      — startup + darling_kernel (the kernel-side DTX hooks).
#   * frameworks/   — CoreGraphics (C), and Objective-C frameworks
#                     (Foundation, UIKit, AVFoundation, CoreAnimation).
#
# The Objective-C `.m` files require an objc-capable compiler (clang or
# gcc+gobjc). We probe for that below; if the host compiler lacks the objc
# frontend we still configure successfully (with a warning) and tag the .m
# files as C with `-x objective-c` so the user gets a single clear error at
# build time rather than a configure-time hard-fail.
# =============================================================================

# C and CXX are already enabled by the top-level project(); we only need to
# add OBJC. We do it conditionally because enable_language(OBJC) is fatal if
# the host compiler can't compile .m files.
if(NOT DEFINED AFROS_HOST_HAS_OBJC)
    set(_objc_probe_dir ${CMAKE_CURRENT_BINARY_DIR}/_objc_probe)
    file(WRITE ${_objc_probe_dir}/probe.m
        "@interface Probe @end\n"
        "@implementation Probe @end\n"
        "int main(void){return 0;}\n")
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -x objective-c -c
                ${_objc_probe_dir}/probe.m
                -o ${_objc_probe_dir}/probe.o
        RESULT_VARIABLE _objc_probe_rc
        OUTPUT_QUIET ERROR_QUIET)
    if(_objc_probe_rc EQUAL 0)
        set(AFROS_HOST_HAS_OBJC TRUE CACHE INTERNAL
            "Host C compiler can build Objective-C sources")
    else()
        set(AFROS_HOST_HAS_OBJC FALSE CACHE INTERNAL
            "Host C compiler can build Objective-C sources")
    endif()
    file(REMOVE_RECURSE ${_objc_probe_dir})
endif()

set(AFROS_INCOMPAT_ENGINE_C_SOURCES
    # --- Mach-O loader ------------------------------------------------------
    macho_loader/loader.c
    macho_loader/macho_parser.c
    macho_loader/symbol_resolver.c
    macho_loader/dyld_emulator.c
    macho_loader/binding_handler.c

    # --- Objective-C runtime (C: implements the ObjC runtime in C) ---------
    runtime/objc_runtime.c
    runtime/class_loader.c
    runtime/message_dispatch.c
    runtime/arc_implementation.c

    # --- Apple filesystems --------------------------------------------------
    filesystem/apfs_emulation.c
    filesystem/hfsplus_emulation.c
    filesystem/bundle_manager.c
    filesystem/icloud_stub.c

    # --- iOS sandbox & entitlements ----------------------------------------
    sandbox/ios_sandbox.c
    sandbox/container_manager.c
    sandbox/entitlements.c
    sandbox/data_protection.c

    # --- Code signing -------------------------------------------------------
    codesign/signature_verifier.c
    codesign/certificate_chain.c
    codesign/provisioning_profile.c

    # --- Darling kernel-side hooks -----------------------------------------
    darling/src/startup/startup.c
    darling/src/kernel/darling_kernel.c

    # --- CoreGraphics (pure C) ---------------------------------------------
    frameworks/CoreGraphics/CGContext.c
    frameworks/CoreGraphics/CGPath.c
    frameworks/CoreGraphics/CGColor.c
    frameworks/CoreGraphics/CGImage.c
)

set(AFROS_INCOMPAT_ENGINE_OBJC_SOURCES
    # --- UIKit --------------------------------------------------------------
    frameworks/UIKIT/UIApplication.m
    frameworks/UIKIT/UIView.m
    frameworks/UIKIT/UIWindow.m
    frameworks/UIKIT/UIViewController.m
    frameworks/UIKIT/UIControl.m

    # --- Foundation ---------------------------------------------------------
    frameworks/Foundation/NSObject.m
    frameworks/Foundation/NSString.m
    frameworks/Foundation/NSArray.m
    frameworks/Foundation/NSDictionary.m
    frameworks/Foundation/NSData.m

    # --- AVFoundation -------------------------------------------------------
    frameworks/AVFoundation/AVPlayer.m
    frameworks/AVFoundation/AVAsset.m
    frameworks/AVFoundation/AVAudioSession.m

    # --- CoreAnimation ------------------------------------------------------
    frameworks/CoreAnimation/CALayer.m
    frameworks/CoreAnimation/CAAnimation.m
    frameworks/CoreAnimation/CATransaction.m
)

if(AFROS_HOST_HAS_OBJC)
    # Preferred path: use the native OBJC language. enable_language() is
    # idempotent — if the top-level CMakeLists.txt already enabled OBJC, this
    # is a no-op.
    enable_language(OBJC)

    add_library(afros-incompat-engine SHARED
        ${AFROS_INCOMPAT_ENGINE_C_SOURCES}
        ${AFROS_INCOMPAT_ENGINE_OBJC_SOURCES}
    )

    # -std=gnu11 is mandatory for the .m files: UIView.m (and others) use
    # `for (NSUInteger i = 0; …)` loop initial declarations, which are only
    # legal in C99 or C11 mode. Some gcc/gobjc builds default to gnu89 and
    # fail with "error: 'for' loop initial declarations are only allowed in
    # C99 or C11 mode". OBJC_STANDARD=11 alone is not always honoured by
    # every CMake release, so we add the flag explicitly via COMPILE_OPTIONS
    # as well.
    target_compile_options(afros-incompat-engine PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX,OBJC>:-std=gnu11>)

    set_target_properties(afros-incompat-engine PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        OBJC_STANDARD 11
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "afros-incompat-engine"
    )
else()
    # Fallback path: the host compiler has no objc frontend. We exclude the
    # .m files from the build entirely — the library will still contain all
    # the C-only components (Mach-O loader, objc runtime C, sandbox, codesign,
    # darling kernel, CoreGraphics). The .m framework wrappers (UIKit,
    # Foundation, CoreAnimation, AVFoundation) require clang or gobjc to
    # build. Users who need full iOS/macOS framework support must install
    # one of those compilers.
    message(WARNING
        "afros-incompat-engine: the host C compiler (${CMAKE_C_COMPILER}) "
        "cannot build Objective-C sources. The .m files in frameworks/ "
        "ARE EXCLUDED from this build. Install clang or gobjc to build "
        "the full library with UIKit/Foundation/CoreAnimation frameworks.")

    add_library(afros-incompat-engine SHARED
        ${AFROS_INCOMPAT_ENGINE_C_SOURCES}
    )

    # Even in the C-only fallback we need gnu11 — some C sources use the
    # same `for` loop initial declarations as the .m files. C_STANDARD=11
    # alone is occasionally ignored by older CMake/gcc combos, so the
    # explicit -std=gnu11 below is belt-and-braces.
    target_compile_options(afros-incompat-engine PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX>:-std=gnu11>)

    set_target_properties(afros-incompat-engine PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "afros-incompat-engine"
    )
endif()

target_include_directories(afros-incompat-engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Make the per-framework headers (e.g. UIKit_AfriOS.h, Foundation_AfriOS.h)
# visible to the .m files inside their own framework directory.
target_include_directories(afros-incompat-engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/UIKIT
    ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/Foundation
    ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/CoreAnimation
    ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/AVFoundation
    ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/CoreGraphics
)

# --- Install rules -----------------------------------------------------------
install(TARGETS afros-incompat-engine
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
