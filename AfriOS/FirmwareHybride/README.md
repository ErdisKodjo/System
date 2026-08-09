# Hybrid Firmware Platform (AfriOS)

## Overview
Hybrid Firmware is a robust, high-security, and power-aware UEFI-based firmware solution designed specifically for the AfriOS ecosystem. It supports multiple architectures (x64, AARCH64, RISCV64) and provides unique features for hardware isolation and energy optimization.

## Key Features
- **Multi-Architecture Support:** Unified codebase for x86_64 and ARM64/RISC-V.
- **Solar-Aware Power Management:** Automatically detects solar power status via ACPI (`_PSR`) to trigger high-performance modes in the AfriOS kernel.
- **Advanced Security:**
    - **Measured Boot:** PCR logging for a verifiable Chain of Trust.
    - **Secure Boot:** Strict policy enforcement for authenticated OS loaders.
- **Minimal Hypervisor (ShimLayer):** Lightweight Type-1 hypervisor for hardware partitioning and enhanced isolation.
- **Robust OTA Updates:** Secure, signed UEFI capsule update engine with A/B slot rollback protection.
- **HII Setup UI:** Interactive multi-tabbed configuration menu for users.

## Project Structure
The project follows the architecture defined in `arbre.txt`:
- `edk2/HybridFirmwarePlatformPkg/`: Main EDK2 package.
    - `PlatformInit/`: SEC/PEI/DXE initialization modules.
    - `HardwareAbstractionLayer/`: ACPI, SMBIOS, and CPU abstraction.
    - `BootManager/`: Intelligent boot selection logic.
    - `Security/`: Secure Boot and Measured Boot implementations.
    - `ShimLayer/`: Hardware virtualization and partitioning.
- `Scripts/`: Automation scripts for build and emulation.
- `docs/`: Technical documentation and whitepapers.

## Getting Started

### Prerequisites
- EDK2 Development Environment (BaseTools, NASM, GCC/Clang cross-compilers).
- QEMU for emulation.

### Building
Build one architecture at a time (le script s'arrête sur la première erreur
plutôt que de continuer silencieusement — voir `docs/architecture_overview.md`
pour le prérequis EDK2 non vendorisé dans ce dépôt) :
```bash
bash Scripts/build.sh X64       # ou AARCH64, ou RISCV64 (défaut : X64)
```
Pour compiler les trois :
```bash
for ARCH in X64 AARCH64 RISCV64; do bash Scripts/build.sh "$ARCH"; done
```

### Running in QEMU
To test the x64 build:
```bash
bash Scripts/run_qemu.sh X64
```
To test the AARCH64 build:
```bash
bash Scripts/run_qemu.sh AARCH64
```

## Contributing
Please refer to `docs/porting_guide.md` for instructions on supporting new hardware platforms.

## License
Copyright (c) 2026, AfriOS Foundation. All rights reserved.
