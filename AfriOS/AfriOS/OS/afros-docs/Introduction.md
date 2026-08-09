# AfriOS Documentation

This is the documentation for the AfriOS kernel and its universal firmware
(`FirmwareHybride`). It follows the style and structure of
[AnduinOS-Docs](https://docs.anduinos.com/) — short, task-oriented pages
linked from here rather than one long README.

## Where to start

- [**Boot**](./Boot.md): what actually happens between power-on and
  `kernel_main()`, across both repositories (firmware then kernel).
- [**Architecture**](./Architecture.md): how the noyau/HAL/drivers/services
  layers and the `ports/` system fit together.
- [**Porting Guide**](../../../../FirmwareHybride/docs/porting_guide.md):
  step-by-step procedure for adding a new CPU architecture or a new board.
- [**Testing**](./Testing.md): verification plan — HAL unit tests, QEMU
  emulation per architecture, firmware boot checklist.

## Where the code actually lives

AfriOS is split across two independent repositories that this documentation
treats as one system:

| Repository | Role |
|---|---|
| `AfriOS-dev_4/.../OS/afros-core/Kernel/` | Le noyau (noyau générique + HAL + drivers + ports par architecture) |
| `FirmwareHybride/edk2/HybridFirmwarePlatformPkg/` | Le firmware UEFI qui amorce la machine avant que le noyau ne prenne le relais |

Le reste de l'arborescence `OS/` (`afros-androsandbox`, `afros-winbridge`,
`afros-harmonygate`, `afros-incompat-engine`) concerne la compatibilité
applicative (faire tourner des apps Android/Windows/HarmonyOS/macOS) — un
axe indépendant de la portabilité matérielle documentée ici.
