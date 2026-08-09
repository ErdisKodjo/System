# État de l'Implémentation - Hybrid Firmware Platform

Ce document résume les travaux d'implémentation restant à effectuer sur le projet FirmwareHybride.

## 1. Composants Non Implémentés (Fichiers Vides)
Les fichiers suivants sont présents dans l'arborescence mais ne contiennent actuellement aucun code (0 octet) :

### Gestion du Démarrage (BootManager)
- `AppleBootHelper.c` & `ConfigPlistParser.c` : Support du boot macOS.
- `WinBootMgr.c` : Support spécifique au boot Windows.
- `IpxeDriver.c` & `HttpBootClient.c` : Boot réseau (PXE/HTTP).
- `BootMenuUi.c` & `BootScripts.c` : Interface utilisateur et scripts du menu de démarrage.

### Mise à jour OTA (OtaUpdate)
- `ABSlotManager.c` : Logique de basculement entre les slots A et B.
- `FwUpdateAgent.c` : Récupération des capsules de mise à jour.

### Diagnostics et Tests
- `MemoryTest.c` : Tests de la mémoire vive au démarrage (POST).
- `PciEnumTest.c` : Énumération et test des périphériques PCI.
- `ShellExtensions.c` : Commandes personnalisées pour le Shell UEFI.

### Abstraction Matérielle (HAL)
- `FdtPlatformDxe.c` : Support du Device Tree (FDT) pour les architectures ARM/RISC-V.

### Virtualisation (ShimLayer)
- `Passthrough.c` : Support du passthrough matériel dans l'hyperviseur minimal.

## 2. Éléments à Finaliser (Stubs et Placeholders)
Certains modules disposent d'une structure mais nécessitent une implémentation réelle :

- **TimerLib** : La fonction `GetPerformanceCounter()` est un stub retournant `0`.
- **AcpiTableGenerator** : L'installation de la table DSDT est simulée (Stub).
- **HII Setup UI** : L'affichage de l'ordre de démarrage (`BootOrderForm.vfr`) est un placeholder.

## 3. Documentation
Les fichiers de documentation suivants doivent être rédigés :
- `docs/architecture_overview.md`
- `docs/porting_guide.md`

## 4. État des Couches de Base
Les composants suivants disposent d'une implémentation initiale fonctionnelle mais peuvent nécessiter des ajustements selon la plateforme :
- **Initialisation (PlatformInit)** : Phases SEC, PEI et DXE.
- **Sécurité (Security)** : Measured Boot et Secure Boot Policy.
- **Hyperviseur (ShimLayer)** : Initialisation VMCS de base.

---
*Dernière mise à jour : 13 mai 2026*
