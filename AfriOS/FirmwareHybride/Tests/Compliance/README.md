# Compliance

Tests de conformité UEFI (ex: suite SCT — UEFI Self-Certification Test) à
exécuter contre l'image `.fd` produite par `Scripts/build.sh`, une fois le
cœur EDK2 vendorisé et un premier build réussi (voir
`docs/architecture_overview.md`).

## Statut

Vide — aucun run de référence n'a encore été possible dans ce dépôt (le
firmware n'a jamais compilé, voir le prérequis EDK2 manquant). Pas de
contenu à fabriquer ici sans un binaire réel à tester contre.

## Quand ce sera pertinent

Après un premier build réussi (`bash Scripts/build.sh X64`) et un boot QEMU
qui atteint BDS (voir `../../docs/architecture_overview.md` et le plan de
test consolidé `../../../AfriOS-dev_4/AfriOS-dev_4/OS/afros-docs/Testing.md`) :

1. Récupérer la suite SCT correspondant à la version UEFI ciblée.
2. La lancer depuis le shell UEFI (`Diagnostics/UefiShell/`, actuellement
   non enregistré dans le `.dsc` — voir `tache.md`) contre l'image buildée.
3. Consigner les résultats ici, par architecture.
