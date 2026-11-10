# AfriOS Build Report

**Generated:** 2026-09-03 14:27:59

## Summary

| Metric | Value |
|--------|-------|
| Build directory | `/home/z/my-project/System/build` |
| Total artifacts | 25 |
| Shared/static libraries | 10 |
| Total size | 4951.8 KB |
| HAL tests | **42 PASS** / 0 FAIL / 2 SKIP |

## Artifacts by kind

### Shared Lib

| Module | Name | Size | SHA-256 (first 12) |
|--------|------|------|---------------------|
| `afros-androsandbox` | `libafros-androsandbox.so` | 1.66 MB | `1b39f518453d` |
| `afros-dxvk` | `d3d11.dll.so` | 433.6 KB | `432469232e4c` |
| `afros-dxvk` | `d3d12.dll.so` | 433.6 KB | `432469232e4c` |
| `afros-dxvk` | `d3d9.dll.so` | 433.6 KB | `432469232e4c` |
| `afros-dxvk` | `dxgi.dll.so` | 433.6 KB | `432469232e4c` |
| `afros-dxvk` | `libafros-dxvk.so` | 433.6 KB | `432469232e4c` |
| `afros-harmonygate` | `libafros-harmonygate.so` | 385.4 KB | `7380239a3fed` |
| `afros-incompat-engine` | `libafros-incompat-engine.so` | 109.0 KB | `51275b52dbef` |
| `afros-winbridge` | `libafros-winbridge.so` | 103.7 KB | `41c50e924c8b` |

### Static Lib

| Module | Name | Size | SHA-256 (first 12) |
|--------|------|------|---------------------|
| `afros-corebridge-core` | `libafros-corebridge.a` | 147.8 KB | `ee0021a5125a` |

### Executable

| Module | Name | Size | SHA-256 (first 12) |
|--------|------|------|---------------------|
| `CMakeFiles` | `Makefile2` | 4.5 KB | `54f5957f2ae3` |
| `CMakeFiles` | `Makefile2` | 4.5 KB | `feb534717333` |
| `CMakeFiles` | `Makefile2` | 4.5 KB | `44b78a2faf2d` |
| `CMakeFiles` | `Makefile2` | 4.3 KB | `19da977d7b43` |
| `CMakeFiles` | `Makefile2` | 4.6 KB | `d2e80beed0e4` |
| `CMakeFiles` | `Makefile2` | 4.6 KB | `62de4a12ca95` |
| `afros-androsandbox` | `Makefile` | 55.9 KB | `5d5b7ff70c08` |
| `afros-corebridge-core` | `Makefile` | 33.9 KB | `4af5a04fba24` |
| `afros-dxvk` | `Makefile` | 35.3 KB | `8b8ef946a9a3` |
| `afros-harmonygate` | `Makefile` | 43.0 KB | `9f114619d420` |
| `afros-incompat-engine` | `Makefile` | 37.0 KB | `02fa397848f1` |
| `afros-winbridge` | `Makefile` | 46.6 KB | `f61f4ee40eee` |
| `hal-tests` | `hal_test_runner` | 30.4 KB | `4f3e0a41007b` |
| `tests` | `fork-exec` | 16.0 KB | `eca1e96b61d6` |
| `tests` | `hello-elf` | 15.6 KB | `4296ad5cf072` |

## HAL test results

- **PASS:** 42
- **FAIL:** 0
- **SKIP:** 2
- Log file: `build/hal-tests/test-output.log`

### Sample test output

```
=== AfriOS HAL Test Runner (P1) ===
Port actif : auto (compilé pour le port sélectionné par AFROS_PORT)

[HAL] Console initialized
[HAL] CPU initialized
[HAL] Memory initialized
[HAL] Interrupts initialized
[HAL] Timer initialized
[HAL] Storage initialized
[PASS] hal_init_cycle
[PASS] hal_get_power_source
[PASS] hal_get_battery_level
[PASS] cpu_get_info_core0
[PASS] cpu_set_frequency_all_cores
[PASS] cpu_wakeup_core
[PASS] port-mcu set_frequency NOT_SUPPORTED
[PASS] port-mcu migrate_task NOT_SUPPORTED
[PASS] memory_alloc
[PASS] memory_free
[PASS] memory_alloc_unique
...
[PASS] device_read_data
[PASS] device_write
[PASS] device_ioctl
[PASS] device_unregister
[PASS] device_unregister_idempotent_fail

=== Summary ===
PASS: 42
FAIL: 0
SKIP: 2
```

## Modules built

| Module | Status | Type |
|--------|--------|------|
| ✅ afros-winbridge | OK | shared lib (.so) |
| ✅ afros-androsandbox | OK | shared lib (.so) |
| ✅ afros-harmonygate | OK | shared lib (.so) |
| ✅ afros-dxvk | OK | shared lib (.so) + 4 Wine DLL aliases |
| ✅ afros-corebridge-core | OK | static lib (.a) |
| ⚠️ afros-incompat-engine | PARTIAL | shared lib (.so, .m excluded — needs gobjc) |
| ✅ hal-tests (port-host-mock) | OK | executable + tests PASS |
| ✅ linux tests | OK | 2 ELF binaries built + run |

## Skipped builds (cross-compilers unavailable)

| Target | Reason | Required toolchain |
|--------|--------|-------------------|
| Kernel ARM64 (port-arm64) | cross-compiler missing | `gcc-aarch64-linux-gnu` |
| Kernel RISC-V (port-riscv) | cross-compiler missing | `gcc-riscv64-linux-gnu` |
| Kernel MCU (port-mcu) | cross-compiler missing | `gcc-arm-none-eabi` |
| Windows tests (.exe) | mingw missing | `x86_64-w64-mingw32-gcc` |
| Firmware UEFI | EDK2 not vendored | `fetch-deps.sh edk2` (~5 GB) |

To enable these builds, install the required toolchains and re-run:
```bash
# Install cross-compilers (Debian/Ubuntu)
sudo apt-get install gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu \
                 gcc-arm-none-eabi x86_64-w64-mingw32-gcc

# Vendor EDK2
./scripts/fetch-deps.sh edk2
```
