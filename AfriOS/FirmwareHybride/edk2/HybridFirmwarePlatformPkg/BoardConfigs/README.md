# Board-specific configurations for HybridFirmwarePlatformPkg

The `HybridFirmwarePlatformPkg.dsc` supports per-board overrides via the
`BOARD_CONFIG` define. Each board has a `*.dsc.include` file in a
subdirectory here that overrides the platform-wide defaults — most
importantly `PcdFdtBaseAddress`, which tells `PlatformInfoPei` where the
Device Tree Blob was loaded by the previous boot stage (bootloader /
OpenSBI / start4.elf).

## Supported boards

| `BOARD_CONFIG`     | Board                         | Arch     | SoC              | FDT base      |
|--------------------|-------------------------------|----------|------------------|---------------|
| `Rpi4`             | Raspberry Pi 4                | AARCH64  | BCM2711 (A72)    | `0x01000000`  |
| `Pine64`           | Pine64 A64                    | AARCH64  | Allwinner A64    | `0x40000000`  |
| `QemuVirt`         | QEMU virt (ARM64)             | AARCH64  | QEMU virt        | `0x40000000`  |
| `QemuVirtRiscv`    | QEMU virt (RISC-V)            | RISCV64  | QEMU virt        | `0x87E00000`  |

## Building for a specific board

```
build -p HybridFirmwarePlatformPkg.dsc -a AARCH64 -t GCC5 \
       -D BOARD_CONFIG=Rpi4
```

For RISC-V:

```
build -p HybridFirmwarePlatformPkg.dsc -a RISCV64 -t GCC5 \
       -D BOARD_CONFIG=QemuVirtRiscv
```

If `BOARD_CONFIG` is **not** passed, the default `PcdFdtBaseAddress` is `0x0`
(see `HybridFirmwarePlatformPkg.dec`), and `FdtPlatformDxe` will fail
gracefully with `EFI_NOT_FOUND` rather than installing an invalid table.
This default is intended for development builds where the FDT is provided
through another mechanism (e.g., a debug shell command).

## How overrides work

When `-D BOARD_CONFIG=<Name>` is passed to `build`, the DSC preprocessor
expands the matching `!include` directive at the bottom of
`HybridFirmwarePlatformPkg.dsc`:

```edk2
!if $(BOARD_CONFIG) == "Rpi4"
  !include BoardConfigs/rpi4/Rpi4.dsc.include
!elseif $(BOARD_CONFIG) == "Pine64"
  !include BoardConfigs/pine64/Pine64.dsc.include
!elseif $(BOARD_CONFIG) == "QemuVirt"
  !include BoardConfigs/qemu-virt/QemuVirt.dsc.include
!elseif $(BOARD_CONFIG) == "QemuVirtRiscv"
  !include BoardConfigs/qemu-virt-riscv/QemuVirtRiscv.dsc.include
!endif
```

The included file may override any PCD or `[Defines]` value — the include
happens **after** the platform-wide defaults, so the board-specific value
wins.

## Adding a new board

1. Create `BoardConfigs/<myboard>/MyBoard.dsc.include` with the overrides
   you need (at minimum `PcdFdtBaseAddress`).
2. Add an `!elseif $(BOARD_CONFIG) == "MyBoard"` branch to the
   `BOARD_CONFIG` switch in `HybridFirmwarePlatformPkg.dsc`.
3. Document the board in the table above.

## Notes on FDT base addresses

- **RPi4**: `start4.elf` loads the FDT at `0x01000000` before jumping to
  UEFI. This is documented in the Raspberry Pi firmware boot flow.
- **Pine64 A64 / Allwinner**: U-Boot's `booti` command loads the FDT at
  `0x40000000` (Allwinner convention).
- **QEMU virt ARM64**: QEMU loads the FDT at the top of RAM and passes
  its address in `x0`. `0x40000000` is the documented default for the
  `virt` machine with `--dtb` not set.
- **QEMU virt RISC-V**: QEMU loads the FDT at the top of RAM (`0x87E00000`
  for 128 MiB at `0x80000000..0x88000000`); OpenSBI receives its address
  in `a1` and forwards it to the next-stage payload.

For boards where the FDT address is only known at runtime (e.g., read
from a register passed by the bootloader), keep `PcdFdtBaseAddress=0x0`
and patch `PlatformInfoPei.c` to read the address from the appropriate
source (`x0` for ARM64 with U-Boot, `a1` for RISC-V with OpenSBI).
