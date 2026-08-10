# AfriOS Project README

## Overview

**AfriOS** is an ambitious, comprehensive multi-platform operating system project that creates a unified runtime environment supporting:

- 🤖 **Android** - Full Android framework and runtime
- 🐧 **Linux** - Native Linux compatibility
- 🌐 **HarmonyOS** - Huawei HarmonyOS support
- 🎮 **Windows** - Via Wine compatibility layer
- 🍎 **iOS/macOS** - Apple platform compatibility
- 🎮 **Gaming/Graphics** - GPU rendering with Vulkan/DirectX

## Project Architecture

```
AfriOS
├── afros-androsandbox (Android Runtime)
├── afros-core (Kernel + HAL)
├── afros-corebridge-core (Runtime Manager)
├── afros-dxvk (Graphics/Vulkan)
├── afros-harmonygate (HarmonyOS Compatibility)
├── afros-incompat-engine (iOS/macOS Compatibility)
├── afros-winbridge (Windows/Wine Support)
```

## Key Components

### 1. Android Runtime (afros-androsandbox)
- ART (Android Runtime) with JIT compilation
- DEX compiler and optimization pipeline
- Binder IPC system
- Android Framework services
- Hardware Abstraction Layer (HAL)

### 2. Kernel (afros-core)
- Custom AfriOS kernel
- Device drivers
- Memory management
- Filesystem support
- Networking stack

### 3. Runtime Manager (afros-corebridge-core)
- Intelligent app format detection
- Multi-platform runtime switching
- Bytecode translation and optimization
- Version compatibility management

### 4. Graphics System (afros-dxvk)
- DirectX to Vulkan translation
- GPU acceleration
- Shader compilation and optimization
- Performance profiling

### 5. HarmonyOS Compatibility (afros-harmonygate)
- ACE UI framework
- Distributed services
- LiteOS kernel support
- HMS integration

### 6. iOS Compatibility (afros-incompat-engine)
- UIKit framework
- Core Animation
- Metal to Vulkan translation
- Mach-O binary loading
- macOS compatibility

### 7. Windows Support (afros-winbridge)
- Wine runtime
- Windows DLL implementations
- Win32 API support
- Direct3D compatibility

## Getting Started

### Prerequisites

- **OS**: Linux, macOS, or Windows (with WSL2)
- **Build Tools**: CMake 3.16+, Make, Python 3.8+
- **Compilers**: GCC 10+, Clang 12+
- **Graphics**: Vulkan SDK

### Installation

```bash
# Clone the repository
git clone https://github.com/AfriOS/AfriOS.git
cd AfriOS

# Install dependencies
./scripts/setup.sh

# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all components
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Building Individual Components

```bash
# Build only Android framework
make android

# Build only kernel
make kernel

# Build graphics subsystem
make graphics

# Build specific compatibility layer
make ios    # Apple compatibility
make windows # Windows support
make harmony # HarmonyOS support
```

## Project Structure

```
.
├── afros-androsandbox/      # Android framework
│   ├── art/                 # ART runtime
│   ├── framework/           # Android framework
│   ├── services/            # System services
│   └── ...
├── afros-core/              # Kernel & HAL
│   ├── Kernel/
│   │   ├── afros/           # Kernel source
│   │   └── hal/             # Hardware abstraction
│   └── ...
├── afros-corebridge-core/   # Runtime manager
├── afros-dxvk/              # Graphics
├── afros-harmonygate/       # HarmonyOS
├── afros-incompat-engine/   # iOS/macOS
├── afros-winbridge/         # Windows/Wine
│
├── common/                  # Shared utilities
├── docs/                    # Documentation
├── scripts/                 # Build scripts
├── tools/                   # Development tools
├── test/                    # Test suite
└── ...
```

## Supported Platforms

| OS | Architecture | Status |
|----|--------------|--------|
| Android | ARM64, ARMv7 | ✅ In Development |
| Linux | x86_64, ARM64 | ✅ In Development |
| HarmonyOS | x86_64, ARM64 | ✅ In Development |
| macOS | x86_64, ARM64 (M1+) | ⏳ Planned |
| iOS | ARM64 | ⏳ Planned |
| Windows | x64 (via Wine) | ⏳ Planned |

## Documentation

- 📖 [Architecture Overview](docs/architecture/overview.md)
- 🔧 [Build System Guide](docs/guides/building.md)
- 🚀 [Getting Started](docs/guides/getting-started.md)
- 📚 [API Documentation](docs/api/index.md)
- ❓ [FAQ](docs/faq/index.md)

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## Development Status

| Component | Status | Progress |
|-----------|--------|----------|
| AfriOS Kernel | In Development | 40% |
| Android Runtime | In Development | 35% |
| Runtime Manager | In Development | 30% |
| Graphics (Vulkan) | In Development | 25% |
| HarmonyOS Compat | Planning | 15% |
| iOS/macOS Compat | Planning | 10% |
| Windows Support | Planning | 5% |

## Performance Targets

- ⚡ **Startup Time**: < 2 seconds
- 💾 **Memory Efficiency**: 40% reduction vs standard Android
- 🎮 **Graphics FPS**: 60 FPS @ 1080p (games)
- 🔄 **JIT Compilation**: < 100ms for typical apps

## Known Issues & Limitations

- Limited device driver support (in development)
- Some NDK APIs not yet implemented
- Graphics on certain GPUs untested
- HarmonyOS support preliminary

## License

This project is licensed under the **Apache License 2.0** - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- **AOSP** - Android Open Source Project
- **Wine Project** - Wine compatibility layer
- **Vulkan Khronos** - Graphics API
- **HarmonyOS** - Huawei HarmonyOS
- **Darling** - macOS/Darwin compatibility

## Contact & Community

- 📧 **Email**: team@afrios.dev
- 💬 **Discord**: [AfriOS Community](https://discord.gg/afrios)
- 🐛 **Issues**: [GitHub Issues](https://github.com/AfriOS/AfriOS/issues)
- 📋 **Discussions**: [GitHub Discussions](https://github.com/AfriOS/AfriOS/discussions)

## Roadmap

### Phase 1 (Current) - Foundation
- ✅ Project structure setup
- 🔄 Core kernel development
- 🔄 Android runtime integration
- 🔄 Basic graphics support

### Phase 2 - Core Features
- 📅 Full Android framework support
- 📅 Multi-platform runtime manager
- 📅 GPU rendering optimization
- 📅 Comprehensive testing suite

### Phase 3 - Compatibility Layers
- 📅 HarmonyOS full support
- 📅 iOS/macOS compatibility
- 📅 Windows/Wine integration
- 📅 Legacy app support

### Phase 4 - Production Readiness
- 📅 Performance optimization
- 📅 Security hardening
- 📅 Extensive documentation
- 📅 Community tooling

---

**Made with ❤️ by the AfriOS Team**

*Building bridges between operating systems.*
