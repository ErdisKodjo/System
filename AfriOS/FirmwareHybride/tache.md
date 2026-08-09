# Liste des Tâches Restantes - Firmware Hybride

## ⛔ Prérequis bloquant (découvert étape 3)
- [ ] Vendoriser le cœur EDK2 réel (`MdePkg`, `MdeModulePkg`, `BaseTools`,
      `ArmPkg`, `RiscVPkg`, etc. sont des dossiers vides dans ce dépôt) —
      rien sous `edk2/` ne peut compiler sans ça, voir
      `docs/architecture_overview.md`.

## 🚀 BootManager (Gestionnaire de Démarrage)
- [ ] Implémenter le support du boot macOS (`AppleBootHelper.c`)
- [ ] Finaliser le parseur de fichiers Config.plist (`ConfigPlistParser.c`)
- [ ] Implémenter le support du boot Windows (`WinBootMgr.c`)
- [ ] Implémenter le pilote iPXE (`IpxeDriver.c`)
- [ ] Implémenter le client HTTP Boot (`HttpBootClient.c`)
- [ ] Développer l'interface graphique du menu de boot (`BootMenuUi.c`)
- [ ] Écrire les scripts de boot (`BootScripts.c`)

## 🔄 OtaUpdate (Mise à jour à distance)
- [ ] Implémenter la gestion des slots A/B et le mécanisme de rollback (`ABSlotManager.c`)
- [ ] Développer l'agent de récupération des capsules (`FwUpdateAgent.c`)

## 🛠️ Diagnostics & Shell
- [ ] Implémenter le test de mémoire POST (`MemoryTest.c`)
- [ ] Implémenter l'énumération et les tests PCI (`PciEnumTest.c`)
- [ ] Développer les extensions de commandes pour le Shell UEFI (`ShellExtensions.c`)
- [ ] Finaliser les scripts de debug (`DebugScripts.nsh`)

## 🏗️ HAL & Virtualisation
- [x] Implémenter le support du Device Tree / FDT (`FdtPlatformDxe.c`) —
      étape 3/4 : détection via `PlatformDetectLib` + HOB
      `gHybridFirmwareFdtBlobHobGuid` publié par `PlatformInfoPei.c`. Reste
      à surcharger `PcdFdtBaseAddress` dans le `.dsc` d'une carte réelle.
- [ ] Implémenter le Passthrough matériel dans l'hyperviseur (`Passthrough.c`)

## 🔧 Finalisation des Stubs & UI
- [x] Remplacer le stub de `GetPerformanceCounter` par une implémentation
      réelle (`TimerLib.c`) — étape 4 : RDTSC (x86_64), `cntpct_el0`
      (AARCH64), `rdtime` (RISC-V), repli logiciel sinon.
- [ ] Finaliser la génération dynamique de la table DSDT (`AcpiTableGenerator.c`)
- [ ] Finaliser le formulaire de gestion de l'ordre de démarrage (`BootOrderForm.vfr`)

## 🔩 Autres corrections (étape 4)
- [x] `SecureBootPolicy.inf` : `FILE_GUID` invalide (contenait `G`, hors
      hex) corrigé, module enregistré dans `[Components]` du `.dsc`
      (existait mais n'était jamais construit).
- [x] `SecureBootPolicy.c` : ajout de l'include manquant `Guid/GlobalVariable.h`
      (`EFI_SECURE_BOOT_MODE_NAME` non résolu sans lui).
- [x] `Scripts/build.sh` : appelait `echo "(Simulated)"` sans jamais lancer
      `build` — appelle maintenant réellement `build -a <ARCH> ...`.
- [ ] Toujours non enregistrés dans `[Components]` (ont un `.c` mais pas de
      `.inf`, ou un `.inf` non listé) : `ShimLayer/`, `OtaUpdate/`,
      `Diagnostics/`, `SetupUi/`, `BootManager/{AppleBoot,WindowsBoot,PxeBoot,LegacyCsm}`.

## 📝 Documentation
- [x] Rédiger la vue d'ensemble de l'architecture (`architecture_overview.md`) — étape 3
- [x] Rédiger le guide de portage plateforme (`porting_guide.md`) — étape 3

## ✅ Vérification (étape 5)
- [x] `Tests/{UnitTests,Compliance,Fuzzing}/` : étaient 3 dossiers vides —
      chacun a maintenant un `README.md` documentant sa portée exacte et ce
      qui bloque (le prérequis EDK2 ci-dessus, pour les trois).
- [x] Plan de vérification consolidé (3 niveaux : tests HAL, QEMU par
      architecture, boot firmware) — voir `../AfriOS-dev_4/AfriOS-dev_4/OS/afros-docs/Testing.md`.
- [ ] Aucune commande de ce plan n'a pu être exécutée dans cet environnement
      (ni `gcc`, ni `qemu` disponibles) — à valider dès que possible sur une
      machine outillée, en commençant par le niveau 1 (tests HAL, ne
      nécessite pas EDK2).

---
*Dernière mise à jour : étape 5 de la recomposition AfriOS.*
