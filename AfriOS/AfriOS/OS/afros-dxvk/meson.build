# SPDX-License-Identifier: MIT
#
# meson.build — AfriOS DXVK (DirectX → Vulkan translation layer).
#
# Builds one shared library, `afros-dxvk.dll.so`, that Wine loads as
# d3d9.dll / d3d11.dll / d3d12.dll / dxgi.dll. The library is self-contained:
# it does not link against a Vulkan SDK at build time; libvulkan.so.1 is
# dlopen'd at runtime via src/vulkan/vulkan_loader.cpp.

project('afros-dxvk', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=2',
    'buildtype=release',
  ],
  license : 'MIT',
)

dxvk_inc = include_directories('include')

# --- Public headers --------------------------------------------------------
install_headers(
  'include/vulkan_loader.h',
  'include/dxvk_adapter.h',
  'include/dxvk_device.h',
  subdir : 'afros-dxvk',
)

# --- Sources ---------------------------------------------------------------
# NOTE: src/d3d12/d3d12_device.app keeps its original (typo'd) extension;
#       meson is told explicitly to compile it as C++ via `override_options`.
dxvk_sources = [
  # D3D9
  'src/d3d9/d3d9.cpp',
  'src/d3d9/d3d9_device.cpp',
  'src/d3d9/d3d9_shader.cpp',
  'src/d3d9/d3d9_state.cpp',
  'src/d3d9/d3d9_swapchain.cpp',
  # D3D11
  'src/d3d11/d3d11_device.cpp',
  'src/d3d11/d3d11_context.cpp',
  'src/d3d11/d3d11_buffer.cpp',
  'src/d3d11/d3d11_texture.cpp',
  'src/d3d11/d3d11_shader.cpp',
  # D3D12
  'src/d3d12/d3d12_command_list.cpp',
  'src/d3d12/d3d12_resource.cpp',
  'src/d3d12/d3d12_description_heap.cpp',
  'src/d3d12/d3d12_device.app',
  # DXGI
  'src/dxgi/dxgi_factory.cpp',
  'src/dxgi/dxgi_adapter.cpp',
  'src/dxgi/dxgi_swapchain.cpp',
  # HLSL front-end
  'src/hlsl/hlsl_compiler.cpp',
  'src/hlsl/hlsl_optimizer.cpp',
  'src/hlsl/spirv_generator.cpp',
  # Vulkan back-end
  'src/vulkan/vulkan_device.cpp',
  'src/vulkan/vulkan_memory.cpp',
  'src/vulkan/vulkan_pipeline.cpp',
  'src/vulkan/vulkan_presenter.cpp',
  # Utilities
  'src/util/cache_manager.cpp',
  'src/util/shader_cache.cpp',
  'src/util/performance_monitor.cpp',
]

# Map the .app extension to C++ so meson treats it as a translation unit.
dxvk_cpp_args = ['-DAFROS_DXVK_BUILD=1']

afros_dxvk = shared_library('afros-dxvk',
  dxvk_sources,
  include_directories : dxvk_inc,
  cpp_args            : dxvk_cpp_args,
  install             : true,
  install_dir         : get_option('libdir') / 'afros-dxvk',
  # libvulkan is loaded lazily via dlopen, so it is NOT a hard link dep.
  # link_with / dependencies intentionally empty.
)

# Treat .app as C++ for the syntax-check / IDE benefit.
meson.add_language('cpp', native : false)

# Make the d3d9/d3d11/d3d12/dxgi DLL aliases Wine expects.
foreach alias : ['d3d9', 'd3d11', 'd3d12', 'dxgi']
  custom_target(alias + '.dll.so',
    output : alias + '.dll.so',
    command : ['ln', '-sf', 'libafros-dxvk.so', '@OUTPUT@'],
    install : true,
    install_dir : get_option('libdir') / 'afros-dxvk',
    depends : afros_dxvk,
  )
endforeach
