# Plan de vérification — QEMU, tests HAL, boot firmware

Livrable de l'étape 5 (Vérification) de la recomposition AfriOS. Trois
niveaux, de la plus rapide à exécuter à la plus complète :

```
1. Tests unitaires HAL (secondes, aucun outil externe)
        ↓
2. Émulation QEMU par architecture (minutes, nécessite QEMU + toolchains croisés)
        ↓
3. Test de boot firmware (minutes, nécessite en plus le cœur EDK2 vendorisé)
```

## Niveau 1 — Tests unitaires HAL

Détail complet, cas par cas : [`Kernel/hal/tests/tests.md`](../afros-core/Kernel/hal/tests/tests.md).

```bash
cd afros-core
./Kernel/hal/scripts/test-kernel.sh          # les 3 ports hébergés (arm64, x86_64, riscv)
./Kernel/hal/scripts/test-kernel.sh riscv    # un seul port
```

Ne nécessite ni QEMU ni toolchain croisé (compile et exécute nativement sur
la machine de dev). C'est la première chose à lancer, et celle qui doit
tourner en CI à chaque commit.

## Niveau 2 — Émulation QEMU par architecture

### Côté noyau (`afros-kernel-sim`)

Le noyau est aujourd'hui un **simulateur hébergé** (`printf`/`stdio.h`, voir
[Boot](./Boot.md)) — pas encore un binaire bare-metal bootable directement
par QEMU en mode système. On peut néanmoins déjà le faire tourner **sous
QEMU en mode utilisateur** (`qemu-<arch>`, pas `qemu-system-<arch>`) une
fois cross-compilé avec les toolchains de l'étape 4 :

```bash
# Exemple arm64, depuis un hôte x86_64
cmake -B build-arm64 -S . -DAFROS_PORT=arm64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/toolchain-arm64.cmake
cmake --build build-arm64

qemu-aarch64 -L /usr/aarch64-linux-gnu \
    build-arm64/afros-core/Kernel/afros/afros-kernel-sim
```
Répéter avec `toolchain-x86_64.cmake` (`qemu-x86_64`) et
`toolchain-riscv.cmake` (`qemu-riscv64`). **Critère de succès** : la
séquence de `hal_init.c` s'affiche en entier sans "ERREUR FATALE", jusqu'à
`[KERNEL] Arret du systeme ou Idle...` (le programme boucle ensuite
indéfiniment par conception — couper avec `timeout` ou Ctrl+C, voir
`test-kernel.sh` qui le fait déjà).

Le port `mcu` n'entre pas dans ce niveau : pas d'exécutable produit (voir
`Kernel/afros/CMakeLists.txt`). Le tester réellement sous
`qemu-system-arm -M <carte Cortex-M>` suppose d'abord la table de vecteurs
et le câblage de `linker.ld` décrits dans [Boot](./Boot.md) — travail non
fait à ce jour.

### Côté firmware (image UEFI complète)

`FirmwareHybride/Scripts/run_qemu.sh` (déjà fonctionnel, pas une simulation)
lance l'image produite par `Scripts/build.sh` :

```bash
cd FirmwareHybride
bash Scripts/build.sh X64          # ou AARCH64 / RISCV64
bash Scripts/run_qemu.sh X64
```

Bloqué aujourd'hui par le prérequis EDK2 documenté dans
`docs/architecture_overview.md` (`Scripts/build.sh` échoue avant même
d'atteindre QEMU tant que le cœur EDK2 n'est pas vendorisé). Une fois ce
prérequis levé, voir Niveau 3 pour ce qu'il faut observer.

## Niveau 3 — Test de boot firmware (SEC → PEI → DXE → BDS)

Checklist de sortie série (`-serial stdio`, déjà activé dans
`run_qemu.sh`) à valider par architecture, en s'appuyant sur les messages
`DEBUG()` réellement présents dans le code (étapes 3-4) :

| Phase | Message attendu | Fichier source |
|---|---|---|
| PEI | `PlatformInfoPei: Power Source = ...` | `PlatformInit/Pei/PlatformInfoPei.c` |
| PEI | `PlatformInfoPei: publication du HOB FDT` (si `PcdFdtBaseAddress` configuré) ou `...pas de HOB FDT publie` | idem |
| DXE | `PlatformInitDxe: Entry - backend de description plateforme : <Acpi\|DeviceTree\|FixedRegister>` | `PlatformInit/Dxe/PlatformInitDxe.c` |
| DXE | `AcpiTableGenerator: Entry (backend detecte : ...)` puis soit `Installing FADT` (si backend Acpi) soit `backend != Acpi, aucune table installee` | `HardwareAbstractionLayer/Acpi/AcpiTableGenerator.c` |
| DXE | `FdtPlatformDxe: Entry (backend detecte : ...)` puis soit installation soit no-op | `HardwareAbstractionLayer/DeviceTree/FdtPlatformDxe.c` |

**Critère de succès minimal** : le backend annoncé par `PlatformInitDxe`
est cohérent avec l'architecture QEMU utilisée (`AARCH64`/`RISCV64` →
`DeviceTree`, `X64` → `Acpi`, sauf PCD de configuration forcés — voir
`PlatformDetectLib`), et exactement un des deux drivers HAL (Acpi ou Fdt)
installe réellement sa table, l'autre restant en no-op.

**Ce que ce niveau ne couvre pas** : le firmware n'atteint pas encore le
noyau AfriOS (BDS ne le charge pas, voir [Boot](./Boot.md)) — "boot
complet" s'arrête à BDS pour l'instant, pas à `kernel_main()`.

## Prérequis par niveau

| Niveau | Outils requis |
|---|---|
| 1 | `cmake`, `gcc` (natif) |
| 2 (noyau) | + `qemu-user` (paquet `qemu-user` sur Debian/Ubuntu), toolchains croisés (`gcc-aarch64-linux-gnu`, etc. — voir `cmake/toolchains/`) |
| 2 (firmware) | + `qemu-system-x86_64/aarch64/riscv64`, **et** le cœur EDK2 vendorisé |
| 3 | identique au niveau 2 (firmware) |

Aucun de ces outils n'est disponible dans l'environnement où cette
recomposition a été effectuée (pas de `gcc`, pas de `qemu`) — ce plan a
donc été conçu et documenté avec précision, mais **aucune de ses commandes
n'a été exécutée avec succès ici**. La prochaine session avec les
toolchains disponibles doit commencer par le Niveau 1.
