# AfriOS Core (Noyau de Base)

## Architecture
AfriOS Core est un micro-noyau con�u pour la performance, l'efficacit� �nerg�tique (Solar-aware) et le support natif des architectures h�t�rog�nes (ARM big.LITTLE).

### Modules Cl�s
- **HAL (Hardware Abstraction Layer)** : Abstraction compl�te du CPU, de la m�moire et des p�riph�riques.
- **CFS (Completely Fair Scheduler)** : Ordonnanceur �quitable optimis� pour les clusters big.LITTLE.
- **Solar-Aware Power Management** : Ajustement dynamique de la performance bas� sur la source d'�nergie (Solaire/Batterie).
- **ZRAM Compression** : Support natif de la compression m�moire pour �conomiser les ressources.

## Instructions d'Int�gration et de Build

### Pr�requis
- Un compilateur crois� pour ARM (ex: `arm-none-eabi-gcc` ou `gcc` local pour simulation).
- `make` pour automatiser la construction.
- `qemu-system-arm` (Optionnel pour tester sur �mulateur).

### Compilation
Pour compiler le noyau sous forme de binaire ELF :
```bash
make
```

### Ex�cution (Mode Simulation)
Pour lancer le noyau et observer la s�quence de d�marrage :
```bash
make run
```

### Nettoyage
```bash
make clean
```

## Structure du Projet
- `Kernel/afros/` : Logique de haut niveau du noyau, arch-agnostic (Scheduler, FS, Network, Power, Security).
- `Kernel/hal/` : Contrats d'abstraction mat�rielle + dispatch g�n�rique - seule fronti�re autoris�e vers le mat�riel.
- `Kernel/drivers/` : Pilotes mat�riels concrets (PCI, ...), d�pendent uniquement de `hal/include/`.
- `Kernel/CMakeLists.txt` : Build unifi� (hal -> drivers -> afros), branch� dans le CMake racine via `AFROS_BUILD_KERNEL`.
- `Makefile` : Build autonome ARM64/simulation (alternative l�g�re au CMake).
- `linker.ld` : D�finition des zones m�moire (FLASH/RAM), dans `Kernel/hal/scripts/`.

Voir `Kernel/hal/docs/architecture.md` pour la justification d�taill�e de
cette s�paration et le d�tail des changements de la recomposition.

---
� 2026 AfriOS Development Team.
