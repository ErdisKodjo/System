# AfriOS - Système d'Exploitation pour le Futur

AfriOS est un écosystème logiciel complet conçu pour la performance, la résilience énergétique et la compatibilité multi-plateforme.

## Structure du Projet

- **OS/afros-core** : Le micro-noyau (Kernel) avec support big.LITTLE et gestion solaire.
- **OS/afros-winbridge** : Couche de compatibilité pour l'exécution d'applications Windows (.exe).
- **OS/afros-power-management** : Système de monitoring et d'optimisation de l'énergie.
- **OS/afros-sdk** : Kit de développement pour créer des applications natives AfriOS.

## Instructions de Configuration et de Build

### 1. Prérequis
- **OS** : Windows, Linux ou macOS.
- **Compilateur** : GCC ARM Cross-compiler (recommandé : `arm-none-eabi-gcc`).
- **Scripts** : PowerShell 7+ pour l'automatisation du build.

### 2. Compilation Globale
Pour compiler l'ensemble de l'écosystème :
```powershell
./afros-build.ps1
```

### 3. Exécution d'un Module Individuel
Chaque module contient son propre Makefile. Pour compiler le noyau uniquement :
```bash
cd OS/afros-core
make
```

## Fonctionnalités Clés
- **Solar-Aware** : Le système ajuste dynamiquement sa consommation en fonction de la source d'énergie (Solaire/Batterie).
- **Hybrid Sandbox** : Capacité d'exécuter des applications Android et Windows côte à côte de manière sécurisée.
- **Micro-Kernel Architecture** : Haute stabilité et isolation des processus.

## Déploiement
Le binaire final (`afros_os.img`) peut être déployé sur :
- **Émulateur** : QEMU (ARM64 Virt machine).
- **Matériel** : Raspberry Pi 4/5, Cartes de développement ARM v8/v9.

---
© 2026 AfriOS Team. Tous droits réservés.
