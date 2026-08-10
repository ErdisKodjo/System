/** @file
  Generic implementation of PlatformDetectLib.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include "PlatformDetectLib.h"
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

PLATFORM_BOOT_BACKEND
EFIAPI
PlatformDetectBootBackend (
  VOID
  )
{
  //
  // Repli générique explicite : une cible sans tables firmware du tout
  // (registre-fixe direct, typiquement un port MCU/bare-metal partageant
  // ce même PlatformInitDxe) force ce choix quel que soit l'architecture.
  //
  if (PcdGetBool (PcdPlatformHasNoFirmwareTables)) {
    DEBUG ((DEBUG_INFO, "PlatformDetect: PcdPlatformHasNoFirmwareTables=TRUE -> FixedRegister\n"));
    return PlatformBackendFixedRegister;
  }

#if defined (MDE_CPU_AARCH64) || defined (MDE_CPU_RISCV64)
  //
  // AARCH64/RISCV64 : plateformes embarquées typiques, Device Tree par
  // défaut (peut être renversé côté ACPI par une intégration future si le
  // firmware constate un RSDP valide, cf. limite connue documentée).
  //
  DEBUG ((DEBUG_INFO, "PlatformDetect: architecture embarquee -> DeviceTree\n"));
  return PlatformBackendDeviceTree;

#elif defined (MDE_CPU_X64) || defined (MDE_CPU_IA32)
  if (PcdGetBool (PcdPreferDeviceTree)) {
    DEBUG ((DEBUG_INFO, "PlatformDetect: PcdPreferDeviceTree=TRUE sur PC-class -> DeviceTree\n"));
    return PlatformBackendDeviceTree;
  }
  DEBUG ((DEBUG_INFO, "PlatformDetect: architecture PC-class -> Acpi\n"));
  return PlatformBackendAcpi;

#else
  DEBUG ((DEBUG_WARN, "PlatformDetect: architecture non reconnue par ce build -> FixedRegister (repli generique)\n"));
  return PlatformBackendFixedRegister;
#endif
}

CONST CHAR8 *
EFIAPI
PlatformBootBackendName (
  IN PLATFORM_BOOT_BACKEND  Backend
  )
{
  switch (Backend) {
    case PlatformBackendDeviceTree:
      return "DeviceTree";
    case PlatformBackendAcpi:
      return "Acpi";
    case PlatformBackendFixedRegister:
      return "FixedRegister";
    default:
      return "Unknown";
  }
}
