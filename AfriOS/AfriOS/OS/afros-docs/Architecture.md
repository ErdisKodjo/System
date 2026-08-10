# Architecture — vue d'ensemble du système AfriOS

Ce document donne la vue "bird's eye" du système complet. Chaque section
renvoie vers le document détaillé correspondant plutôt que de dupliquer son
contenu.

## Les deux moitiés du système

```
                    ┌─────────────────────────────────────┐
                    │  FirmwareHybride (EDK2, UEFI)        │
                    │  amorce la machine                   │
                    └───────────────┬───────────────────────┘
                                    │ BDS charge le noyau
                    ┌───────────────▼───────────────────────┐
                    │  afros-core/Kernel (AfriOS)           │
                    │  noyau + HAL + drivers + ports         │
                    └─────────────────────────────────────┘
```

Elles sont conçues pour être portables **indépendamment** l'une de l'autre :
le firmware sait démarrer x86_64/AARCH64/RISC-V via son propre mécanisme
(macros `MDE_CPU_*`, `SUPPORTED_ARCHITECTURES` du `.dsc`) ; le noyau sait
tourner sur arm64/x86_64/riscv/mcu via son propre mécanisme (`ports/`,
option `AFROS_PORT`). Voir [Boot](./Boot.md) pour comment elles s'articulent
dans le temps.

## Côté firmware — détail dans FirmwareHybride/docs/architecture_overview.md

- 4 phases PI (SEC → PEI → DXE → BDS).
- Détection de plateforme et choix de backend (Device Tree / ACPI / registre
  fixe) via `PlatformDetectLib`, avec repli générique explicite.
- Un point d'entrée par architecture dans `MultiArchResetVector.S`.
- **Prérequis bloquant documenté** : le cœur EDK2 n'est pas vendorisé dans
  ce dépôt.

## Côté noyau — détail dans Kernel/hal/docs/architecture.md

Séparation stricte en quatre couches, chacune ne dépendant que de la
suivante (jamais l'inverse) :

| Couche | Rôle | Sait-elle quelle architecture tourne ? |
|---|---|---|
| `services/` (racine `OS/`, ex: `afros-network`) | démons espace utilisateur | Non |
| `afros/` (noyau générique) | ordonnanceur, mémoire, réseau, énergie, fs, sécurité | Non |
| `drivers/` | pilotes concrets (PCI, ...) | Non |
| `hal/` | contrats + dispatch générique | Non — délègue au port actif |
| `ports/port-<arch>/` | **seul** endroit où l'architecture apparaît | Oui, c'est son unique rôle |

Cette dernière ligne est la règle absolue du système : si un besoin
matériel nouveau ne rentre dans aucune des six tables d'ops de `hal/`, la
réponse est d'ajouter une abstraction (nouveau header dans `hal/include/`),
jamais une branche `#ifdef ARCH` dans `afros/` ou `drivers/`.

## Les six abstractions matérielles

CPU, mémoire (MMU incluse), contrôleur d'interruptions, timer, console/UART,
stockage — chacune définie une fois dans `hal/include/*_abstraction.h`,
implémentée quatre fois (une par port). Le tableau comparatif des quatre
ports (contrôleur IRQ, timer, console, stockage utilisés par chacun) est
dans [`Kernel/ports/README.md`](../afros-core/Kernel/ports/README.md).

## Ce qui n'est PAS dans ce document

- Les couches de compatibilité applicative (`afros-androsandbox`,
  `afros-winbridge`, `afros-harmonygate`, `afros-incompat-engine`) : un axe
  orthogonal (faire tourner des apps étrangères), pas la portabilité
  matérielle du noyau lui-même.
- Le détail des six abstractions et de leurs quatre implémentations : voir
  `Kernel/ports/README.md`.
- La checklist "ajouter une plateforme" : voir le
  [Guide de portage](../../../../FirmwareHybride/docs/porting_guide.md).
