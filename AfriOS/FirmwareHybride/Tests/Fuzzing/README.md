# Fuzzing

Cibles candidates au fuzzing une fois le firmware buildable, par ordre de
priorité (surface d'attaque = code qui parse une entrée externe non
fiable) :

1. **`AcpiTableGenerator.c` / `FdtPlatformDxe.c`** (`HardwareAbstractionLayer/`)
   — parsent potentiellement des tables/blobs fournis par un bootloader
   précédent, donc une entrée externe non fiable par nature. Priorité haute :
   c'est exactement le genre de code où AFL/LibFuzzer trouvent des bugs
   réels sur de vrais firmwares UEFI.
2. **`BootManager/AppleBoot/ConfigPlistParser.c`** — parseur de fichier
   texte (plist), surface classique. Non implémenté à ce jour (voir
   `tache.md`) : à fuzzer une fois écrit, pas avant.
3. **`OtaUpdate/CapsuleEngine.c`** — parse des capsules de mise à jour
   signées reçues potentiellement d'un réseau non fiable.

## Statut

Vide — aucune cible n'est encore compilable (prérequis EDK2 manquant, voir
`docs/architecture_overview.md`), et une bonne partie du code ci-dessus
(`ConfigPlistParser.c`, `CapsuleEngine.c`) est elle-même encore un stub.
Ce fichier fixe l'ordre de priorité pour quand ce sera le cas, plutôt que
de laisser le dossier sans intention documentée.
