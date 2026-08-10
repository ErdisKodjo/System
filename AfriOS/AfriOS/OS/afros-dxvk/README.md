# afros-dxvk

AfriOS's port of [DXVK](https://github.com/doitsujin/dxvk) — the
DirectX 9 / 10 / 11 / 12 → Vulkan translation layer — built so Windows games
running under AfriOS's Wine bridge can render against a native Vulkan ICD
instead of falling back to OpenGL.

## Layout

```
afros-dxvk/
├── meson.build                  # build definition
├── include/
│   ├── vulkan_loader.h          # minimal Vulkan loader + handle types
│   ├── dxvk_adapter.h           # DxvkAdapter (wraps VkPhysicalDevice)
│   └── dxvk_device.h            # DxvkDevice  (wraps VkDevice + queue)
└── src/
    ├── d3d9/   # IDirect3D9 / Device / Shader / State / SwapChain
    ├── d3d11/  # ID3D11Device / Context / Buffer / Texture / Shader
    ├── d3d12/  # ID3D12Device / CommandList / Resource / DescriptorHeap
    ├── dxgi/   # IDXGIFactory / Adapter / SwapChain
    ├── hlsl/   # HLSL → AST → SPIR-V (compiler / optimizer / generator)
    ├── vulkan/ # DxvkDevice + memory allocator + pipeline cache + presenter
    └── util/   # on-disk cache / shader cache / perf monitor
```

Each translation layer sits on top of a single `DxvkDevice` (declared in
`include/dxvk_device.h`). The D3D / DXGI layers never call `vk*` directly —
they go through the high-level factory helpers on `DxvkDevice`, which keeps
all real Vulkan entry-point resolution concentrated in `src/vulkan/*.cpp`.

## Entry points exported to Wine

| Symbol                  | Header   | Purpose                              |
|-------------------------|----------|--------------------------------------|
| `Direct3DCreate9`       | d3d9     | D3D9 factory (`IDirect3D9`)          |
| `Direct3DCreate9Ex`     | d3d9     | D3D9Ex factory                       |
| `CreateDXGIFactory`     | dxgi     | DXGI factory                         |
| `CreateDXGIFactory1`    | dxgi     | DXGI factory (extended)              |
| `D3D11CreateDevice`     | d3d11    | D3D11 device                         |
| `D3D12CreateDevice`     | d3d12    | D3D12 device                         |

All entry points are `extern "C"` so Wine can resolve them by name when the
built shared library is dropped in as `d3d9.dll` / `d3d11.dll` / `d3d12.dll` /
`dxgi.dll`.

## Self-contained build

The port compiles **without** a Vulkan SDK installed: `include/vulkan_loader.h`
re-declares the minimal subset of `Vk*` handle types, `VkResult`, and
`VkFormat` / `VkExtent2D` / `VkViewport` / etc. that the translation units
need, and `src/vulkan/vulkan_private.h` re-declares the `Vk*CreateInfo`
structs + `PFN_vk*` typedefs the back-end uses. Real Vulkan entry points are
resolved at runtime via `dlopen("libvulkan.so.1")`. When the loader fails
(libvulkan absent), every factory degrades to returning sentinel handles and
the D3D entry points return `E_FAIL`, so callers can fall back gracefully.

Per-file syntax check (no linking required):

```
g++ -fsyntax-only -std=c++17 -I include src/<dir>/<file>.cpp
g++ -fsyntax-only -std=c++17 -I include -x c++ src/d3d12/d3d12_device.app
```

## Caches

* **Pipeline cache** — `/var/cache/afros-dxvk/pipeline.bin` (raw
  `VkPipelineCache` blob, loaded at startup, flushed periodically).
* **Shader cache** — `/var/cache/afros-dxvk/shaders/<xxhash64>.spv`, keyed by
  `XXH64(hlsl_source || entry || profile || flags)`. In-memory LRU (1024
  entries) fronts the disk cache.

## Status

Skeleton / first implementation pass. All 27 source files are populated and
syntax-clean; the translation paths are wired end-to-end but the actual vk
calls resolve through function pointers obtained from the loader, and most
`vkCmd*` recording is documented as the next step (the command-buffer plumbing
exists; the per-draw state flush is a no-op). Next milestone: wire a real
`libvulkan` ICD + port DXVK's `dxbc_compiler` for full SM5 → SPIR-V coverage.

## License

MIT (see `LICENSE`). The bundled `XXH64` in `src/util/shader_cache.cpp` is the
reference implementation under the BSD-2-Clause license.
