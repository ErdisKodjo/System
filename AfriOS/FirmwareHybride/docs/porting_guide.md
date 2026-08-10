# Guide de portage — ajouter une nouvelle plateforme à AfriOS

Porter AfriOS sur une nouvelle plateforme matérielle touche deux dépôts
distincts et indépendants : le firmware (`FirmwareHybride`, EDK2) qui
amorce la machine, et le noyau (`AfriOS-dev_4/.../afros-core/Kernel`) qui
prend le relais. Ce guide couvre les deux, dans l'ordre où ils s'exécutent.

## Partie 1 — Firmware (`FirmwareHybride/edk2/HybridFirmwarePlatformPkg`)

### 1.1 Nouvelle architecture CPU (ex: ajouter ARMv7 32 bits)

1. Ajouter l'architecture à `SUPPORTED_ARCHITECTURES` dans
   `HybridFirmwarePlatformPkg.dsc`.
2. `PlatformInit/Sec/MultiArchResetVector.S` : ajouter un bloc
   `#elif defined(__arm__) ...` avec la logique d'entrée bas niveau
   (désactivation des interruptions, init pile, transition PEI).
3. `HardwareAbstractionLayer/CpuHal/CpuHalLib.c` : ajouter un bloc
   `#elif defined (MDE_CPU_ARM)` dans `CpuHalGetInfo` avec la topologie
   réelle de la cible (nombre de cœurs, fréquence, big.LITTLE ou non).
4. `HardwareAbstractionLayer/PlatformDetect/PlatformDetectLib.c` : décider
   si cette architecture doit préférer Device Tree ou ACPI par défaut
   (ajouter la branche `#if defined (MDE_CPU_ARM)` au bon endroit).

### 1.2 Nouvelle carte/SoC sur une architecture déjà supportée

Pas besoin de toucher au reset vector ni à `CpuHalLib` si l'architecture
existe déjà. Il suffit de :

1. Fournir le blob Device Tree (ou les tables ACPI) spécifiques à la carte —
   `FdtPlatformDxe.c` attend que la phase SEC/PEI ait renseigné
   `gPlatformFdtBlobBase` (actuellement toujours 0, voir gap connu dans
   `architecture_overview.md` — c'est le premier endroit à câbler).
2. Si la carte n'a ni ACPI ni Device Tree exploitable (bring-up minimal,
   MCU) : positionner `PcdPlatformHasNoFirmwareTables|TRUE` dans le `.dsc`
   de la plateforme et fournir les valeurs `PcdFixed*` nécessaires — c'est
   le repli générique, déjà géré par `PlatformDetectLib`.
3. Enregistrer les modules `PlatformInit/Pei/*`, `PlatformInit/Dxe/*` et
   `HardwareAbstractionLayer/*` nécessaires dans `[Components]` du `.dsc`
   (voir la liste des composants non enregistrés dans
   `architecture_overview.md`, section "Gap pré-existant").
4. `bash Scripts/build.sh` puis `bash Scripts/run_qemu.sh <ARCH>` pour
   valider en émulation avant tout matériel réel.

### 1.3 Prérequis absolu

Le cœur EDK2 (`MdePkg`, `MdeModulePkg`, `BaseTools`, et `ArmPkg`/`RiscVPkg`
pour ces architectures) doit être vendorisé — voir
`architecture_overview.md`. Sans ça, aucune étape ci-dessus ne compile.

## Partie 2 — Noyau (`afros-core/Kernel/ports/`)

### 2.1 Ajouter un port pour une architecture déjà couverte par la HAL

C'est le cas le plus fréquent : la HAL (`hal/include/*_abstraction.h`) n'a
pas besoin de changer, seul un nouveau `ports/port-<nom>/` est nécessaire.

1. `mkdir -p Kernel/ports/port-<nom>/src`
2. Implémenter les six fichiers, un par table d'ops (voir
   `ports/README.md` pour le tableau des interfaces) :
   `cpu_port.c`, `memory_port.c`, `interrupt_port.c`, `timer_port.c`,
   `console_port.c`, `storage_port.c` — chacun définit la variable globale
   correspondante (`arch_cpu_ops`, `arch_memory_ops`, ...) déclarée
   `extern` dans le header d'abstraction associé.
3. Copier `ports/port-arm64/CMakeLists.txt` en changeant uniquement le
   commentaire d'en-tête — le nom de cible (`afros-port`) et la liste de
   sources restent identiques par convention.
4. Si une opération n'a pas de sens sur cette plateforme (ex : pas de MMU,
   mono-cœur), retourner `AFROS_ERROR_NOT_SUPPORTED` plutôt que d'inventer
   un comportement — voir `port-mcu/` pour l'exemple de référence.
5. Builder : `cmake -B build -DAFROS_PORT=<nom> && cmake --build build`
   (ou `make PORT=<nom>` pour le build ARM64-legacy autonome).

### 2.2 Ajouter un besoin matériel qui n'existe dans aucune table d'ops

Si le besoin ne rentre dans aucune des six abstractions existantes (ex :
un contrôleur DMA, un capteur), **ne pas** contourner la HAL depuis
`Kernel/afros/` ou `Kernel/drivers/`. Ajouter une nouvelle abstraction :

1. Nouveau header `hal/include/<domaine>_abstraction.h`, même forme que
   les six existants (struct d'ops + `extern <type>_ops_t arch_<domaine>_ops;`).
2. L'inclure dans `hal/include/afros_hal.h`.
3. Chaque port existant doit fournir une implémentation (même minimale,
   retournant `AFROS_ERROR_NOT_SUPPORTED` si non applicable) — sinon
   l'édition de liens échoue pour les ports qui ne l'implémentent pas dès
   qu'un composant du noyau appelle cette table d'ops.

### 2.3 Checklist avant de considérer un port "prêt"

- [ ] Les 6 fichiers de `ports/port-<nom>/src/` compilent (`cmake --build`).
- [ ] `afros-kernel-sim` démarre et exécute la séquence de `hal_init.c`
      (console → CPU → mémoire → IRQ → timer) sans retour d'erreur.
- [ ] Toute opération non supportée renvoie `AFROS_ERROR_NOT_SUPPORTED`,
      jamais un succès simulé trompeur.
- [ ] `ports/README.md` mis à jour (tableau des ports disponibles).
