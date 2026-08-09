# Architecture — Hybrid Firmware Platform

Ce document décrit l'architecture du firmware universel d'AfriOS
(`FirmwareHybride/edk2/HybridFirmwarePlatformPkg`) telle qu'établie à
l'étape 3 de la recomposition AfriOS. Il complète `README.md` (vue
d'ensemble) et `tache.md` (liste de tâches).

## Prérequis manquant — cœur EDK2 non vendorisé

**Avant toute autre chose** : `edk2/MdePkg`, `MdeModulePkg`, `BaseTools`,
`UefiCpuPkg`, `NetworkPkg`, `SecurityPkg`, `ArmPkg`, `RiscVPkg`,
`OtaUpdatePkg`, `SignedCapsulePkg` sont des **répertoires vides** dans ce
workspace. Aucun cœur EDK2 réel (BaseLib, Uefi.h, BaseTools de compilation)
n'est vendorisé. Rien sous `edk2/` — y compris le code déjà présent avant
l'étape 3 — n'a donc jamais pu être réellement compilé. C'est un prérequis
plus fondamental que n'importe quel gap fonctionnel listé ci-dessous :

```bash
git clone --branch edk2-stable202405 https://github.com/tianocore/edk2.git /tmp/edk2-upstream
# puis fusionner MdePkg/, MdeModulePkg/, BaseTools/, UefiCpuPkg/, etc.
# avec les répertoires vides existants (ne pas toucher à HybridFirmwarePlatformPkg/,
# ArmPkg/ et RiscVPkg/ ont aussi besoin de leurs pendants upstream réels).
```

Tout ce qui suit décrit une architecture *prête à compiler* une fois ce
prérequis résolu — elle a été conçue en respectant fidèlement les
conventions EDK2 (types EFI_STATUS, macros `MDE_CPU_*`, sections .inf/.dsc)
mais n'a pas pu être vérifiée par une compilation réelle dans cet
environnement (pas de toolchain EDK2 disponible).

## Les quatre phases de boot (PI : Platform Initialization)

```
SEC  (PlatformInit/Sec/MultiArchResetVector.S)
  └─ point d'entrée matériel brut, un branchement par architecture
     (x86_64 / AARCH64 / RISC-V RV64), pas encore de pile ni de C.
      ↓
PEI  (PlatformInit/Pei/{PlatformInfoPei, MemoryInit})
  └─ détection minimale (source d'alimentation), init mémoire.
      ↓
DXE  (PlatformInit/Dxe/{PlatformInitDxe, AcpiPlatformDxe},
      HardwareAbstractionLayer/{Acpi,DeviceTree,Smbios,CpuHal},
      BootManager/, Security/, ShimLayer/, OtaUpdate/, SetupUi/)
  └─ la majorité de la logique "universelle" vit ici.
      ↓
BDS  (choix du chargeur d'OS via BootManager/)
```

## Détection de plateforme et choix du backend (étape 3)

C'est le cœur de la demande "boot universel". Avant l'étape 3,
`AcpiTableGenerator.c` installait la FADT **sans condition**, sur
n'importe quelle architecture — y compris RISC-V/embarqué, où ACPI n'a
souvent pas de sens et où `FdtPlatformDxe.c` était un fichier **vide**
(aucune implémentation, pas de `.inf`, jamais construit).

### Nouveau composant : `PlatformDetectLib`

`HardwareAbstractionLayer/PlatformDetect/PlatformDetectLib.{h,c,inf}`
expose `PlatformDetectBootBackend()`, appelée par `PlatformInitDxe`,
`AcpiTableGenerator` et `FdtPlatformDxe` :

```
PcdPlatformHasNoFirmwareTables == TRUE ?
  └─ oui → FixedRegister (repli générique, toute architecture)
  └─ non → architecture cible :
       AARCH64 / RISCV64            → DeviceTree (sauf PcdPreferDeviceTree=FALSE forcé... voir note)
       X64 / IA32                   → Acpi (sauf PcdPreferDeviceTree=TRUE)
       autre / inconnue à ce build   → FixedRegister (repli générique)
```

- `AcpiTableGenerator.c` retourne immédiatement (`EFI_SUCCESS`, no-op) si le
  backend détecté n'est pas `Acpi`.
- `FdtPlatformDxe.c` (désormais implémenté) retourne immédiatement si le
  backend détecté n'est pas `DeviceTree` ; sinon il installe le blob FDT
  comme table de configuration UEFI sous `gHybridFirmwareFdtTableGuid`
  (GUID propre à ce dépôt : `EmbeddedPkg`, qui définirait normalement
  `gFdtTableGuid`, n'est pas vendorisé ici — voir section précédente).
- Le repli générique (`FixedRegister`) correspond aux cibles qui n'ont ni
  ACPI ni Device Tree (MCU/bare-metal) : aucun des deux drivers n'agit,
  `PlatformInitDxe`/`CpuHalLib` consomment directement les `PcdFixed*`
  du build.

### Limite connue : détection au build, pas au runtime

`PlatformDetectBootBackend()` décide **à la compilation** (macros
d'architecture + PCD `PcdsFixedAtBuild`), pas en sondant le matériel réel au
démarrage. Une vraie détection runtime lirait :
- le registre `x0`/`a1` légué par un bootloader précédent (protocole de boot
  Linux : pointeur FDT) pour confirmer la présence d'un Device Tree ;
- la signature `"RSD PTR "` en mémoire basse / EFI System Table pour
  confirmer ACPI.

C'est un travail d'étape 4 (bring-up sur matériel/QEMU réel), pas de
conception — la mécanique de sélection et de repli (`PlatformDetectLib`,
le no-op conditionnel des deux drivers) est déjà en place et n'aurait qu'à
être alimentée par une détection plus précise sans changer son interface.

## Multi-architecture ailleurs dans le code

| Fichier | Mécanisme |
|---|---|
| `PlatformInit/Sec/MultiArchResetVector.S` | `#if defined(__x86_64__) / __aarch64__ / __riscv`, un point d'entrée par arch |
| `HardwareAbstractionLayer/CpuHal/CpuHalLib.c` | `#if defined(MDE_CPU_AARCH64/X64/IA32/RISCV64)`, topologie différenciée |
| `HybridFirmwarePlatformPkg.dsc` | `SUPPORTED_ARCHITECTURES = IA32\|X64\|AARCH64\|RISCV64` (mécanisme natif EDK2, pas besoin d'un `ports/` séparé — voir note ci-dessous) |

**Note sur `ports/`** : contrairement à `afros-core/Kernel` (qui n'avait
aucun mécanisme de sélection par architecture avant l'étape 3), EDK2 fournit
déjà nativement la sienne (`build -a <ARCH>` + sections `[LibraryClasses.<Arch>]`
dans le `.dsc`). Créer un dossier `ports/` parallèle ici aurait dupliqué ce
que l'écosystème EDK2 fait déjà — la bonne pratique EDK2 est le branchement
par macro à l'intérieur d'une bibliothèque partagée, comme dans `CpuHalLib.c`.

## Gap pré-existant, hors périmètre de l'étape 3

Sur les 12 fichiers `.inf` du package, seuls 10 composants sont dans
`[Components]` du `.dsc` (+FdtPlatformDxe ajouté à l'étape 3). Les
sous-systèmes suivants ont du code source mais **aucun `.inf`** et ne sont
donc jamais construits, quel que soit l'état du `.dsc` : `ShimLayer/`
(hyperviseur minimal), `OtaUpdate/` (A/B + capsules), `Diagnostics/`
(POST, shell), `SetupUi/` (HII), `BootManager/AppleBoot`, `WindowsBoot`,
`PxeBoot`, `LegacyCsm`, `Security/SecureBoot` (a un `.inf` mais n'est pas
listé dans `[Components]`). C'est un axe différent de la portabilité
matérielle (choix d'OS à démarrer, pas support de CPU) — recensé dans
`tache.md`, à traiter en étape 4.
