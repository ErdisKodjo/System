# AfriOS Project Structure

## Overview
AfriOS is a comprehensive multi-platform operating system project designed to provide unified runtime and compatibility layers across Android, Linux, HarmonyOS, Windows (via Wine), and Apple platforms.

## Repository Structure

### 1. **afros-androsandbox** - Android Runtime Implementation
Core Android framework and runtime environment.
- `art/` - Android Runtime (ART), garbage collection, JIT compilation
- `compiler/` - DEX compilation and optimization
- `binder/` - Inter-process communication
- `framework/` - Android Framework services
- `hardware/` - Hardware Abstraction Layer (HAL)
- `ndk/` - NDK libraries (libc, libm, libdl)
- `services/` - System services
- `surfaceflinger/` - Graphics rendering
- `system/` - Core system components

### 2. **afros-core** - Kernel and HAL
Low-level kernel and hardware abstraction.
- `Kernel/afros/` - Custom AfriOS kernel
  - `arch/` - Architecture-specific code
  - `drivers/` - Device drivers
  - `fs/` - Filesystem implementations
  - `mm/` - Memory management
  - `net/` - Networking subsystem
  - `ipc/` - Interprocess communication
- `Kernel/hal/` - Hardware abstraction layer
  - `audio/`, `display/`, `input/`, `power/`, `wifi/`, `thermal/`

### 3. **afros-corebridge-core** - Runtime Manager & Loader
Multi-platform runtime management and application loading.
- `loader/` - Intelligent app format detection and loading
- `runtime_managers/` - Platform-specific runtime managers
  - `android_runtime_manager.cpp`
  - `linux_runtime_manager.c`
  - `win_runtime_manager.c`
  - `ios_runtime_manager.cpp`
  - `harmony_runtime_manager.c`
- `unified_execution/` - Bytecode translation and optimization
- `version_management/` - Version compatibility tracking
- `tools/` - CLI, debugger, profiler

### 4. **afros-dxvk** - GPU Rendering & Vulkan
Graphics rendering and DirectX to Vulkan translation.
- `src/d3d11/`, `src/d3d12/` - DirectX implementations
- `src/dxgi/` - DXGI layer
- `src/vulkan/` - Vulkan backend
- `shaders/` - HLSL and SPIR-V shaders
- `benchmarks/` - Performance benchmarks

### 5. **afros-harmonygate** - HarmonyOS Compatibility
HarmonyOS runtime and framework compatibility.
- `ability/` - HarmonyOS Ability framework
- `ace/` - ACE engine (UI framework)
- `distributed/` - Distributed services
- `liteos/` - LiteOS kernel components
- `hms/` - HarmonyOS Mobile Services

### 6. **afros-incompat-engine** - Apple/iOS Compatibility
iOS and macOS application compatibility layer.
- `AVFoundation/` - Media framework
- `CoreAnimation/` - Animation framework
- `CoreGraphics/` - Graphics framework
- `UIKit/` - UI framework
- `darling/` - Darwin compatibility layer
- `macho_loader/` - Mach-O binary loading
- `sandbox/` - iOS sandboxing

### 7. **afros-winbridge** - Windows/Wine Support
Windows application support via Wine.
- `wine/loader/` - Wine application loader
- `wine/dlls/` - Windows DLLs (kernel32, user32, gdi32, etc.)
- `wine/programs/` - Windows programs shim
- `wine/server/` - Wine server implementation

## Build System

### Supported Build Systems
- **Android**: Android.bp (Blueprint)
- **HarmonyOS**: BUILD.gn (GN)
- **DXVK**: meson.build (Meson)
- **Core Bridge**: CMakeLists.txt (CMake)

## Compilation Targets

1. Android/AfriOS native compilation
2. Linux x86_64, ARM64
3. HarmonyOS x86_64, ARM64
4. Apple M1/M2 (ARM64), Intel (x86_64)
5. Windows x86, x64 (via Wine)

## Key Features

✅ Multi-platform runtime management
✅ Unified bytecode execution
✅ Cross-platform app compatibility
✅ GPU rendering with Vulkan backend
✅ Distributed computing support
✅ Comprehensive Hardware Abstraction
✅ Security and sandboxing

## Development Guidelines

### Directory Naming Conventions
- Use lowercase for directory names
- Use underscores for multi-word names
- Platform-specific code in platform subdirectories

### File Organization
- Source files (.c, .cpp, .cc) in `src/`
- Headers in `include/`
- Tests in `tests/`
- Documentation in `docs/`

## Documentation Files

- `art-modifications.md` - ART runtime modifications
- `binder-architecture.md` - Binder IPC architecture
- `compatibility.md` - Platform compatibility notes
- `architecture.md` - Overall system architecture
- `runtime-api.md` - Runtime API documentation

## Getting Started

See individual README.md files in each repository for specific setup instructions.
