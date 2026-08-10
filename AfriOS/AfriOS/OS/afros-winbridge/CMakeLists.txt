# CMakeLists.txt for afros-winbridge
# =============================================================================
# afros-winbridge is AfriOS's Windows compatibility layer (a stripped-down,
# AfriOS-aware fork of Wine). It is shipped as a single shared library,
# libafros-winbridge.so, that bundles:
#
#   * PE loader   — parses & relocates Windows PE binaries, resolves DLL
#                   imports and translates them to ELF-side symbols.
#   * Filesystem  — Windows drive letters, NTFS attribute emulation and
#                   path translation onto AfriOS VFS paths.
#   * Registry    — hive file I/O + an in-process registry emulator.
#   * Syscalls    — NT-style syscall table + I/O manager + kernel-object
#                   emulation.
#   * Server      — mini wineserver (process / registry coordination).
#   * Services    — SCM, Windows Installer, Event Log, Task Scheduler.
#   * COM         — proxy/stub marshaling, type libraries, ActiveX.
#   * Loader      — preloader + main wine_loader entry point.
#   * Cache       — JIT, shader, DLL and registry caches.
#
# The library is pure C, threads-heavy (-lpthread), and links on a stock
# Linux host (it talks to the AfriOS HAL only optionally through afros-hal).
# =============================================================================

set(AFROS_WINBRIDGE_SOURCES
    # --- PE loader ----------------------------------------------------------
    wine/pe_loader/pe_parser.c
    wine/pe_loader/pe_to_elf.c
    wine/pe_loader/dll_resolver.c
    wine/pe_loader/resource_loader.c

    # --- Filesystem translation --------------------------------------------
    wine/filesystem/path_translator.c
    wine/filesystem/file_attributes.c
    wine/filesystem/drive_manager.c
    wine/filesystem/ntfs_emulation.c

    # --- Registry hive emulator --------------------------------------------
    wine/registry/hive_manager.c
    wine/registry/hive_io.c
    wine/registry/system_hive.c
    wine/registry/software_hive.c
    wine/registry/sam_hive.c
    wine/registry/registry_emulator.c

    # --- NT syscall surface -------------------------------------------------
    wine/syscall/nt_syscall_table.c
    wine/syscall/syscall_translator.c
    wine/syscall/io_manager.c
    wine/syscall/kernel_objects.c

    # --- Wineserver ---------------------------------------------------------
    wine/server/wineserver.c
    wine/server/process.c
    wine/server/registry.c

    # --- Windows services ---------------------------------------------------
    wine/services/service_control_manager.c
    wine/services/windows_installer.c
    wine/services/event_log.c
    wine/services/task_scheduler.c

    # --- COM runtime --------------------------------------------------------
    wine/com/com_runtime.c
    wine/com/proxy_stub.c
    wine/com/marshaler.c
    wine/com/type_library.c
    wine/com/activex_support.c

    # --- Wine loader --------------------------------------------------------
    wine/loader/preloader.c
    wine/loader/wine_loader.c

    # --- Caches -------------------------------------------------------------
    wine/cache/jit_cache.c
    wine/cache/shader_cache.c
    wine/cache/dll_cache.c
    wine/cache/registry_cache.c
)

add_library(afros-winbridge SHARED ${AFROS_WINBRIDGE_SOURCES})

set_target_properties(afros-winbridge PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "afros-winbridge"
)

# Public headers: include/ (top-level, may not exist on every checkout) plus
# wine/include/ which holds winbridge.h, pe_loader.h, registry.h, wine_compat.h.
target_include_directories(afros-winbridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/wine/include
)

# -lpthread: the wineserver + service control manager both spin up background
# threads. Use Threads::Threads (the portable wrapper around -lpthread).
if(NOT TARGET Threads::Threads)
    find_package(Threads REQUIRED)
endif()
target_link_libraries(afros-winbridge PUBLIC Threads::Threads)

# --- Install rules -----------------------------------------------------------
install(TARGETS afros-winbridge
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY wine/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
