# Boot — du power-on à `kernel_main()`

Ce document trace la séquence complète de démarrage d'AfriOS, à travers les
deux dépôts. Chaque étape renvoie vers le fichier source réel qui
l'implémente — voir [Architecture](./Architecture.md) pour le découpage en
couches.

## Vue d'ensemble

```
[Firmware — FirmwareHybride/edk2/HybridFirmwarePlatformPkg/]

  SEC   PlatformInit/Sec/MultiArchResetVector.S
          └─ point d'entrée matériel brut, un branchement par architecture
             (x86_64 / AARCH64 / RISC-V)
  PEI   PlatformInit/Pei/{PlatformInfoPei, MemoryInit}
          └─ détection source d'alimentation, init mémoire, publie le HOB
             FDT si PcdFdtBaseAddress est configuré pour la carte
  DXE   PlatformInit/Dxe/PlatformInitDxe
          └─ appelle PlatformDetectLib : choisit DeviceTree / Acpi /
             FixedRegister (voir HardwareAbstractionLayer/PlatformDetect/)
        HardwareAbstractionLayer/{Acpi/AcpiTableGenerator, DeviceTree/FdtPlatformDxe}
          └─ seul le driver correspondant au backend détecté agit
  BDS   BootManager/*
          └─ sélectionne et charge le prochain binaire (dont, pour AfriOS,
             le noyau AfriOS lui-même)

                              ↓ transfert de contrôle

[Noyau — AfriOS-dev_4/.../OS/afros-core/Kernel/]

  main()          Kernel/afros/main.c
                    └─ kernel_main()
  hal_init()      Kernel/hal/src/hal_init.c — séquence stricte :
                    1. arch_console_ops.init()     (console/UART en premier :
                                                     seul canal de sortie si
                                                     une étape suivante échoue)
                    2. arch_cpu_ops.init()
                    3. arch_memory_ops.init()
                    4. arch_interrupt_ops.init()
                    5. arch_timer_ops.init(100)
  kernel_main()   suite : power_check_solar_status() → afros_cfs_init() →
                    afros_cfs_run() → memory_reclaim_pages() → boucle idle
```

## Ce qui est réel aujourd'hui vs. simulé

- **Firmware** : les phases SEC/PEI/DXE existent comme code source
  EDK2-conforme, mais **rien ne peut compiler** tant que le cœur EDK2
  (`MdePkg`, `BaseTools`, ...) n'est pas vendorisé dans ce dépôt — voir
  `FirmwareHybride/docs/architecture_overview.md`.
- **Noyau** : `afros-kernel-sim` est un **simulateur hébergé** — `main()`
  standard, `printf`/`stdio.h`, tourne comme un programme espace-utilisateur
  ordinaire sous l'OS hôte (Linux/Windows). La séquence ci-dessus s'exécute
  réellement (`cmake --build` puis lancer le binaire), mais aucune ligne ne
  touche un vrai registre matériel — chaque port (`Kernel/ports/port-*/`)
  imprime ce qu'il *ferait* sur un vrai périphérique.
- Le transfert firmware → noyau (BDS charge et saute dans le noyau AfriOS)
  n'est pas câblé : ce sont deux binaires distincts, construits et exécutés
  séparément aujourd'hui (le noyau ne tourne pas encore *sur* le firmware).

## Rendre ce boot réel (bare-metal)

Prérequis, dans l'ordre :

1. Vendoriser le cœur EDK2 (bloquant pour tout le côté firmware).
2. Remplacer `printf`/`stdio.h` dans `Kernel/afros/main.c` et chaque
   `console_port.c` par de vrais accès UART (le contrat existe déjà :
   `console_abstraction.h`) — le noyau devient freestanding.
3. Écrire un point d'entrée bare-metal réel (table de vecteurs, `_start`,
   init `.data`/`.bss`) et câbler `Kernel/hal/scripts/linker.ld`
   (actuellement écrit mais non référencé par aucun build).
4. Faire en sorte que `BootManager/` du firmware charge effectivement ce
   binaire noyau plutôt qu'un OS générique.

Chacun de ces points est détaillé dans le
[Guide de portage](../../../../FirmwareHybride/docs/porting_guide.md).
