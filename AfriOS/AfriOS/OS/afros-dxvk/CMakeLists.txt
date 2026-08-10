# CMakeLists.txt for afros-dxvk
# =============================================================================
# afros-dxvk is the AfriOS DirectX -> Vulkan translation layer, modelled on
# upstream DXVK. It builds as a single shared library `libafros-dxvk.so` that
# Wine loads under four well-known DLL names (d3d9.dll, d3d11.dll, d3d12.dll,
# dxgi.dll) so unmodified Windows games find a DirectX entry point. The
# library is self-contained: it does NOT link against a Vulkan SDK at build
# time — libvulkan.so.1 is dlopen'd at runtime via the prototypes declared in
# include/vulkan_loader.h (and re-declared in src/vulkan/*.cpp).
#
# NOTE: src/d3d12/d3d12_device.app is a C++ translation unit that keeps its
# original (typo'd) extension; we set_source_files_properties to compile it
# as CXX so CMake does the right thing.
# =============================================================================

set(AFROS_DXVK_SOURCES
    # D3D9
    src/d3d9/d3d9.cpp
    src/d3d9/d3d9_device.cpp
    src/d3d9/d3d9_shader.cpp
    src/d3d9/d3d9_state.cpp
    src/d3d9/d3d9_swapchain.cpp

    # D3D11
    src/d3d11/d3d11_device.cpp
    src/d3d11/d3d11_context.cpp
    src/d3d11/d3d11_buffer.cpp
    src/d3d11/d3d11_texture.cpp
    src/d3d11/d3d11_shader.cpp

    # D3D12
    src/d3d12/d3d12_device.app
    src/d3d12/d3d12_command_list.cpp
    src/d3d12/d3d12_resource.cpp
    src/d3d12/d3d12_description_heap.cpp

    # DXGI
    src/dxgi/dxgi_factory.cpp
    src/dxgi/dxgi_adapter.cpp
    src/dxgi/dxgi_swapchain.cpp

    # HLSL front-end
    src/hlsl/hlsl_compiler.cpp
    src/hlsl/hlsl_optimizer.cpp
    src/hlsl/spirv_generator.cpp

    # Vulkan back-end
    src/vulkan/vulkan_device.cpp
    src/vulkan/vulkan_memory.cpp
    src/vulkan/vulkan_pipeline.cpp
    src/vulkan/vulkan_presenter.cpp

    # Utilities
    src/util/cache_manager.cpp
    src/util/shader_cache.cpp
    src/util/performance_monitor.cpp
)

# Treat the .app file as C++ so CMake compiles it instead of ignoring it.
# `LANGUAGE CXX` alone is not enough: gcc/clang decide compile-vs-link by
# file extension, and `.app` is not a recognised source extension, so we
# also pass `-x c++` to force the compiler to treat it as a C++ translation
# unit regardless of the file's name.
set_source_files_properties(src/d3d12/d3d12_device.app PROPERTIES
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++")

add_library(afros-dxvk SHARED ${AFROS_DXVK_SOURCES})

set_target_properties(afros-dxvk PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "afros-dxvk"
)

target_include_directories(afros-dxvk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build-define so headers can switch between export/import declarations.
target_compile_definitions(afros-dxvk PRIVATE AFROS_DXVK_BUILD=1)

# IMPORTANT: libvulkan.so is NOT linked here. The Vulkan loader is dlopen()'d
# at runtime via the self-declared prototypes in src/vulkan/*.cpp. This keeps
# afros-dxvk buildable on systems that lack the Vulkan SDK.

# --- Wine DLL aliases --------------------------------------------------------
# Wine loads graphics DLLs by their well-known Windows names. Create symlinks
# <name>.dll.so -> libafros-dxvk.so for d3d9 / d3d11 / d3d12 / dxgi so the
# build artifact can be dropped straight into Wine's lib directory.
set(AFROS_DXVK_ALIASES d3d9 d3d11 d3d12 dxgi)
foreach(_alias IN LISTS AFROS_DXVK_ALIASES)
    add_custom_command(
        TARGET afros-dxvk POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                libafros-dxvk.so
                ${CMAKE_CURRENT_BINARY_DIR}/${_alias}.dll.so
        COMMENT "Creating Wine DLL alias ${_alias}.dll.so -> libafros-dxvk.so"
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_alias}.dll.so
            DESTINATION lib/afros-dxvk)
endforeach()

# --- Install rules -----------------------------------------------------------
install(TARGETS afros-dxvk
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
