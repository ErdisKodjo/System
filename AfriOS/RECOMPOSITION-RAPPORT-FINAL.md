# AfriOS — Rapport final de recomposition

Synthèse des 5 étapes de la mission (audit, recomposition, firmware
universel, complétion, vérification). Détails complets dans les documents
liés ; ce fichier consolide les 6 livrables demandés à l'origine.

---

## 1. Arborescence annotée (état final)

```
System/
├── build.ps1                          [NOUVEAU, étape 3] orchestrateur --Target dispatch CMake+EDK2
├── RECOMPOSITION-RAPPORT-FINAL.md     [ce fichier]
│
├── AfriOS-dev_4/AfriOS-dev_4/OS/
│   ├── afros-docs/                     [NOUVEAU, étape 4-5] Introduction, Boot, Architecture, Testing
│   ├── cmake/toolchains/               [NOUVEAU, étape 4] 4 toolchains de cross-compilation
│   ├── test/CMakeLists.txt             [NOUVEAU, étape 5] corrige un bug de config CMake root
│   └── afros-core/
│       ├── Makefile                    [MODIFIÉ] PORT=, fichiers manquants ajoutés, symboles dédupliqués
│       └── Kernel/
│           ├── CMakeLists.txt          [NOUVEAU] agrège hal → ports/port-<arch> → drivers → afros (+tests)
│           ├── afros/                  [INCHANGÉ dans son contenu] noyau générique
│           │   └── main.c              [CORRIGÉ] doublons de symboles retirés
│           ├── hal/
│           │   ├── include/            [+4 headers] interrupt/timer/console/storage_abstraction.h
│           │   ├── src/                [+device_manager.c, -cpu/memory_manager.c déplacés]
│           │   └── tests/              [NOUVEAU, étape 5] hal_smoke_test.c + tests.md
│           ├── drivers/                [EXTRAIT de afros/drivers/, étape 2]
│           │   └── pci/afros_pci.c     [CORRIGÉ, étape 4] s'enregistre vraiment auprès du device_manager
│           └── ports/                  [NOUVEAU, étape 3] port-arm64, port-x86_64, port-riscv, port-mcu
│
└── FirmwareHybride/
    ├── tache.md                        [MIS À JOUR] items cochés, prérequis EDK2 ajouté en tête
    ├── docs/
    │   ├── architecture_overview.md    [NOUVEAU, étape 3] vue d'ensemble + prérequis EDK2 manquant
    │   └── porting_guide.md            [NOUVEAU, étape 3] procédure noyau + firmware
    ├── Tests/{UnitTests,Compliance,Fuzzing}/README.md  [NOUVEAU, étape 5] portée documentée
    ├── Scripts/build.sh                [CORRIGÉ, étape 4] appelle réellement `build`, plus de simulation
    └── edk2/HybridFirmwarePlatformPkg/
        ├── HardwareAbstractionLayer/
        │   ├── PlatformDetect/         [NOUVEAU, étape 3] PlatformDetectLib
        │   ├── DeviceTree/FdtPlatformDxe.c  [IMPLÉMENTÉ, étape 3-4] était vide
        │   ├── Acpi/AcpiTableGenerator.c    [MODIFIÉ, étape 3] conditionnel au backend détecté
        │   └── CpuHal/CpuHalLib.c      [MODIFIÉ, étape 3] topologie différenciée par arch
        ├── PlatformInit/
        │   ├── Sec/MultiArchResetVector.S  [MODIFIÉ, étape 3] branche RISC-V ajoutée
        │   ├── Pei/PlatformInfoPei.c   [MODIFIÉ, étape 4] publie le HOB FDT
        │   └── Dxe/PlatformInitDxe.c   [MODIFIÉ, étape 3] journalise le backend détecté
        ├── Security/SecureBoot/        [CORRIGÉ+ENREGISTRÉ, étape 4] GUID invalide, absent du .dsc
        └── Libraries/TimerLib/TimerLib.c   [MODIFIÉ, étape 4] GetPerformanceCounter réel par arch
```

## 2. Tableau de correspondance module par module

| Domaine | Avant (étape 1) | Après (étape 5) |
|---|---|---|
| Séparation noyau/HAL/drivers | `drivers/` enfoui dans `afros/drivers/`, doublon vide dans `hal/drivers/` | `Kernel/{afros,hal,drivers,ports}/` — 4 couches indépendantes, règle de dépendance à sens unique documentée |
| Build noyau | Makefile ARM64 seul, jamais réellement lié (symboles dupliqués) ; CMake ne référençait pas le noyau | CMake unifié (`AFROS_PORT`), Makefile corrigé, les deux compilent (en principe — non vérifié faute d'outillage) |
| Portabilité matérielle noyau | Une seule implémentation HAL, codée en dur ARM64 | 4 ports normalisés, interface CMake identique (`afros-port`), 6 tables d'ops par port |
| Détection plateforme firmware | `AcpiTableGenerator` installait la FADT sans condition, `FdtPlatformDxe.c` vide | `PlatformDetectLib` + repli générique, les deux drivers HAL conditionnels |
| Multi-arch firmware | Reset vector x86_64/AARCH64 seulement ; `CpuHalLib` une seule heuristique pour tous | + branche RISC-V ; topologie par macro `MDE_CPU_*` |
| Tests | `hal/tests/tests.md` vide, `OS/test/` cassait la config CMake, `FirmwareHybride/Tests/` vide | Tests HAL réels + CTest, bug CMake corrigé, portée documentée pour Compliance/Fuzzing/UnitTests firmware |
| Documentation | `architecture_overview.md`/`porting_guide.md` vides, aucun doc "Boot" | 6 documents réels (`afros-docs/*`, `FirmwareHybride/docs/*`) |

## 3. HAL + firmware universel — couverture

ARM64, x86_64, RISC-V **et** MCU (bonus, non demandé mais cohérent avec le
système de ports) — voir `Kernel/ports/README.md` pour le tableau détaillé
par contrôleur.

## 4. Build unifié
```
./build.ps1 -Target arm64|x86_64|riscv|mcu [-Cross] [-KernelOnly|-FirmwareOnly]
```

## 5. Guide de portage
`FirmwareHybride/docs/porting_guide.md` — noyau (`ports/port-<nom>/`) et
firmware (nouvelle architecture ou nouvelle carte).

---

## 6. Liste des tâches restantes, priorisées

### P0 — Bloquant, à faire avant tout le reste
1. **Vendoriser le cœur EDK2 réel** (`MdePkg`, `MdeModulePkg`, `BaseTools`,
   `ArmPkg`, `RiscVPkg`, ...) dans `FirmwareHybride/edk2/` — rien côté
   firmware ne compile sans ça.
2. **Exécuter le plan de vérification** (`afros-docs/Testing.md`) sur une
   machine avec `gcc`/`cmake`/`qemu` — rien n'a pu être testé dans cet
   environnement. Commencer par le niveau 1 (tests HAL, pas besoin d'EDK2).

### P1 — Haute valeur, débloque beaucoup de choses
3. Rendre le noyau AfriOS **freestanding** : remplacer `printf`/`stdio.h`
   dans `Kernel/afros/main.c` et chaque `console_port.c` par de vrais accès
   `arch_console_ops` — condition pour un boot bare-metal réel.
4. Écrire la table de vecteurs + `crt0` pour `port-mcu` et câbler
   `Kernel/hal/scripts/linker.ld` (existe, jamais référencé par un build).
5. Renseigner `PcdFdtBaseAddress` pour une carte réelle si le backend
   `DeviceTree` est utilisé (le mécanisme HOB existe, la valeur reste à 0
   par défaut).
6. Automatiser les cas `[ ]` de `Kernel/hal/tests/tests.md` (tous
   réalisables dès aujourd'hui, sans QEMU).

### P2 — Réel, mais pas bloquant
7. Enregistrer `ShimLayer/OtaUpdate/Diagnostics/SetupUi/BootManager/{AppleBoot,WindowsBoot,PxeBoot,LegacyCsm}`
   dans le `.dsc` une fois leur code complété (axe "choix d'OS", pas
   portabilité CPU).
8. Détection de plateforme **runtime** réelle (scan RSDP / registre FDT)
   au lieu de la détection au build actuelle (`PlatformDetectLib`).
9. Génération dynamique de la DSDT, `BootOrderForm.vfr`, `Passthrough.c`.
10. Corriger les chemins d'inclusion relatifs fragiles de
    `afros-network`/`afros-storage`/`afros-power-management`
    (`../afros-core/Kernel/hal/include`, survivrait mal à un futur
    déplacement de `hal/`).

### P3 — Axe séparé (compatibilité applicative, pas portabilité matérielle)
11. `afros-androsandbox`, `afros-winbridge`, `afros-harmonygate`,
    `afros-incompat-engine` : ~73% de fichiers vides, mission distincte de
    celle traitée ici (délibérément laissés hors périmètre depuis l'étape 2).
12. Incohérence de nommage historique `AFRIOS_*` (options CMake racine) vs
    `AFROS_*` (options introduites ici, alignées sur `afros-core`) —
    cosmétique, sans impact fonctionnel.
