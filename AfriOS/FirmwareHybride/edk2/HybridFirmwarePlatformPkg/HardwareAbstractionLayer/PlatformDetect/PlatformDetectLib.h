/** @file
  Platform boot-backend detection for Hybrid Firmware.

  Décide, avant que PlatformInitDxe/AcpiTableGenerator/FdtPlatformDxe ne
  s'exécutent, quelle description matérielle le firmware doit publier :
  Device Tree, tables ACPI, ou un repli "registre fixe" générique quand
  aucune des deux n'est pertinente (cible MCU/bare-metal sans firmware
  tables). Voir docs/architecture_overview.md pour la justification.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#ifndef PLATFORM_DETECT_LIB_H_
#define PLATFORM_DETECT_LIB_H_

typedef enum {
  PlatformBackendDeviceTree,
  PlatformBackendAcpi,
  PlatformBackendFixedRegister
} PLATFORM_BOOT_BACKEND;

/**
  Determines which firmware description backend this platform should use.

  Détection basée sur l'architecture cible (macros MDE_CPU_*) et deux PCD de
  configuration (PcdPreferDeviceTree, PcdPlatformHasNoFirmwareTables) — pas
  une sonde matérielle dynamique : voir la note "Limite connue" dans
  docs/architecture_overview.md pour la trajectoire vers une détection
  runtime réelle (RSDP scan / registre a1 FDT).

  @return Le backend à utiliser. Ne renvoie jamais de valeur indéfinie :
          en dernier recours (architecture non reconnue), retourne
          PlatformBackendFixedRegister — c'est le repli générique.
**/
PLATFORM_BOOT_BACKEND
EFIAPI
PlatformDetectBootBackend (
  VOID
  );

/**
  Convenience helper for DEBUG logging.

  @param[in] Backend  The backend value to describe.

  @return A static, null-terminated ASCII string naming the backend.
**/
CONST CHAR8 *
EFIAPI
PlatformBootBackendName (
  IN PLATFORM_BOOT_BACKEND  Backend
  );

#endif
