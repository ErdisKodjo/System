# UnitTests

Destiné aux tests unitaires **host-based** des bibliothèques du firmware
(ex: `PlatformDetectLib`, `TimerLib`), via le framework EDK2
`UnitTestFrameworkPkg` (compile et exécute sur l'hôte de build, sans QEMU
ni matériel — c'est la même logique que les tests hébergés côté noyau, voir
`afros-core/Kernel/hal/tests/`).

## Statut

Vide à ce jour. Bloqué par le même prérequis que tout le reste de
`edk2/` : `UnitTestFrameworkPkg` fait partie du cœur EDK2 non vendorisé
dans ce dépôt (voir `docs/architecture_overview.md`).

## Premier test à écrire une fois le cœur EDK2 vendorisé

`PlatformDetectLibUnitTest.c` — le meilleur candidat : `PlatformDetectLib`
est une bibliothèque `BASE` pure (pas de dépendance PEI/DXE), déterministe,
sans accès matériel. Cas à couvrir, correspondant directement à la logique
de `PlatformDetectLib.c` (voir `../../edk2/HybridFirmwarePlatformPkg/HardwareAbstractionLayer/PlatformDetect/`) :

- `PcdPlatformHasNoFirmwareTables=TRUE` → retourne toujours `PlatformBackendFixedRegister`, quelle que soit l'architecture de build.
- Build AARCH64/RISCV64, `PcdPlatformHasNoFirmwareTables=FALSE` → `PlatformBackendDeviceTree`.
- Build X64/IA32, `PcdPreferDeviceTree=FALSE` → `PlatformBackendAcpi`.
- Build X64/IA32, `PcdPreferDeviceTree=TRUE` → `PlatformBackendDeviceTree`.
- `PlatformBootBackendName()` ne retourne jamais `NULL` pour aucune valeur de l'enum.

Squelette (à adapter au nom réel des macros `UNIT_TEST_*` une fois
`UnitTestFrameworkPkg` disponible pour vérifier la syntaxe exacte) :

```c
UNIT_TEST_STATUS
EFIAPI
TestFixedRegisterOverride (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  PatchPcdSetBool (PcdPlatformHasNoFirmwareTables, TRUE);
  UT_ASSERT_EQUAL (PlatformDetectBootBackend (), PlatformBackendFixedRegister);
  return UNIT_TEST_PASSED;
}
```
