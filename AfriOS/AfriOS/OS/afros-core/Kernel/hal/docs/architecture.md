# Architecture d'afros-core/Kernel

Ce document justifie la séparation en quatre couches appliquée lors de la
recomposition d'AfriOS (étape 2). Il ne couvre que le socle noyau
(`afros-core/Kernel/`) ; les couches de compatibilité applicative
(`afros-androsandbox`, `afros-winbridge`, `afros-harmonygate`,
`afros-incompat-engine`) sont un axe orthogonal, hors périmètre.

## Les quatre couches

```
┌─────────────────────────────────────────────────────────┐
│ services/   afros-network, afros-storage,                │
│             afros-power-management (racine OS/)          │
│             → démons espace utilisateur, politiques       │
│               visibles par l'utilisateur/l'admin.         │
├─────────────────────────────────────────────────────────┤
│ afros/      noyau générique (arch-agnostic)               │
│             scheduler/, memory/, network/, power/,        │
│             fs/, security/                                │
│             → logique réactive en contexte noyau ;        │
│               ne touche jamais le matériel directement.   │
├─────────────────────────────────────────────────────────┤
│ drivers/    pilotes matériels concrets (pci/, ...)         │
│             → implémentent device_ops_t, s'enregistrent   │
│               auprès de la HAL.                            │
├─────────────────────────────────────────────────────────┤
│ hal/        contrats d'abstraction + dispatch générique   │
│             include/*_abstraction.h, src/*_manager.c      │
│             → SEULE frontière autorisée vers le matériel. │
│               Implémentée par port (arch/x86_64, arm64,   │
│               riscv, mcu — étape 3).                       │
└─────────────────────────────────────────────────────────┘
```

## Règle de dépendance (à sens unique)

```
services/  →  afros/ (via appels systèmes/IPC, pas encore implémentés)
services/  →  hal/include/  (accès direct pour les besoins de supervision, ex. batterie)
afros/     →  hal/include/
drivers/   →  hal/include/
hal/src/   →  port matériel actif (arch/<plateforme>, étape 3)
```

Aucune couche ne remonte : `hal/` ne connaît ni `afros/`, ni `drivers/`, ni
`services/`. C'est ce qui rend la HAL portable indépendamment du reste.

## Pourquoi cette séparation (et pas celle d'AnduinOS)

L'audit comparatif (étape 1) a établi qu'AnduinOS-1.4 est un respin Ubuntu
sans noyau, HAL ni pilotes propres — il n'offre aucune structure à recopier
à ce niveau. La séparation ci-dessus suit donc le pattern générique
noyau/HAL/drivers/services (comparable à Linux ou Zephyr), déjà amorcé dans
le code existant via les tables d'opérations (`cpu_ops_t`, `memory_ops_t`,
`device_ops_t`, `hal_ops_t` — voir `hal/include/`).

## Ce qui a changé lors de la recomposition (étape 2)

1. **`drivers/` extrait de `afros/drivers/`** vers `Kernel/drivers/`, au même
   niveau que `afros/` et `hal/` — auparavant les pilotes étaient enfouis
   dans le noyau générique, sans cible de build indépendante.
2. **`hal/drivers/` (stub vide) supprimé** — doublon ambigu avec le point 1 ;
   les pilotes vivent désormais à un seul endroit.
3. **CMakeLists.txt ajoutés** à `Kernel/`, `Kernel/hal/`, `Kernel/afros/`,
   `Kernel/drivers/` — la HAL n'avait jusqu'ici aucun système de build CMake
   (fichier vide), et le noyau n'était compilable que via le `Makefile`
   autonome d'`afros-core`, absent du build CMake unifié malgré l'option
   `AFRIOS_BUILD_KERNEL` déjà déclarée à la racine.
4. **`afros-core/Kernel` branché dans `OS/CMakeLists.txt`** — l'option
   `AFRIOS_BUILD_KERNEL` existait sans effet ; le noyau était absent du
   build "unifié" que le README annonce.
5. **`afros-core/Makefile` complété** — plusieurs fichiers présents sur
   disque (`big_little.c`, `big_little_support.c`, `power_aware_sched.c`,
   `numa_balancer.c`, `tcp_westwood_africa.c`, `intermittent_net.c`,
   `protocol_compression.c`, `brownout_protection.c`,
   `opportunistic_compute.c`, `opportunistic_sleep.c`) n'étaient jamais
   compilés.
6. **Doublons de symboles corrigés dans `afros/main.c`** — `afros_cfs_init`,
   `afros_cfs_run` et `power_check_solar_status` étaient redéfinis
   localement dans `main.c` alors que `scheduler/afros_cfs.c` et
   `power/solar_aware.c` en contiennent déjà l'implémentation réelle. Ce
   doublon empêchait toute édition de liens réussie dès que les modules
   étaient compilés ensemble — le noyau n'avait donc, à notre
   connaissance, jamais été réellement construit dans son ensemble.
   `main.c` déclare désormais ces fonctions en `extern` et appelle les
   implémentations réelles.
7. **Services (`afros-network`, `afros-storage`, `afros-power-management`)
   liés explicitement à la cible `afros-hal`** — ils incluaient déjà les
   en-têtes HAL mais ne liaient jamais la bibliothèque définissant les
   symboles globaux qu'ils appellent (`afros_hal_ops`).

## Ce qui reste hors périmètre de l'étape 2 (renvoyé aux étapes 3-4)

- Le noyau (`afros/main.c` → `kernel_main`) reste un **simulateur hébergé**
  (`printf`/`stdio.h`, `int main()`), pas un point d'entrée bare-metal. Le
  rendre freestanding (vecteur d'interruption, init MMU, `_start` sans
  libc) est un prérequis de l'étape 3 (firmware universel / ports).
- `hal/include/device_abstraction.h` n'a pas encore d'implémentation
  (`src/device_manager.c` manquant) — le pilote PCI ne peut pas s'enregistrer
  réellement tant que ce fichier n'existe pas.
- Aucun répertoire `ports/` n'existe encore : `hal/src/*.c` contient une
  implémentation unique et simulée (ARM64 big.LITTLE codé en dur dans les
  commentaires et les valeurs), pas encore un dispatch vers un port
  sélectionné à la compilation ou au démarrage.
