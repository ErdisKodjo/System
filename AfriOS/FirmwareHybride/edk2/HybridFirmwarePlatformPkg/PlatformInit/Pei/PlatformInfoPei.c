/** @file
  Platform Information PEI Module.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiPei.h>
#include <Library/PeimEntryPoint.h>
#include <Library/PeiServicesLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>

/**
  Entry point for PlatformInfoPei.

  @param[in]  FileHandle  Handle of the file being invoked.
  @param[in]  PeiServices Describes the list of possible PEI Services.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
PlatformInfoPeiEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  UINT8  PowerSource;

  DEBUG ((DEBUG_INFO, "PlatformInfoPei: Entry\n"));

  PowerSource = PcdGet8 (PcdPowerSourceType);

  switch (PowerSource) {
    case 0:
      DEBUG ((DEBUG_INFO, "PlatformInfoPei: Power Source = AC\n"));
      break;
    case 1:
      DEBUG ((DEBUG_INFO, "PlatformInfoPei: Power Source = Battery\n"));
      break;
    case 2:
      DEBUG ((DEBUG_INFO, "PlatformInfoPei: Power Source = SOLAR (Optimal)\n"));
      break;
    default:
      DEBUG ((DEBUG_WARN, "PlatformInfoPei: Unknown Power Source (%d)\n", PowerSource));
      break;
  }

  // In a real platform, this would also detect memory size, CPU topology, etc.
  // and publish HOBs for the DXE phase.

  //
  // Étape 4 : publie l'adresse du blob FDT (si la plateforme en a renseigné
  // une via PcdFdtBaseAddress) pour que FdtPlatformDxe puisse la relire en
  // phase DXE. PcdFdtBaseAddress|0x0 par défaut => aucun HOB publié => le
  // driver DXE échoue proprement (EFI_NOT_FOUND) plutôt que d'installer un
  // blob invalide (voir HardwareAbstractionLayer/DeviceTree/FdtPlatformDxe.c).
  //
  {
    UINT64  FdtBase;

    FdtBase = PcdGet64 (PcdFdtBaseAddress);
    if (FdtBase != 0) {
      DEBUG ((DEBUG_INFO, "PlatformInfoPei: publication du HOB FDT (base = 0x%lx)\n", FdtBase));
      BuildGuidDataHob (&gHybridFirmwareFdtBlobHobGuid, &FdtBase, sizeof (FdtBase));
    } else {
      DEBUG ((DEBUG_INFO, "PlatformInfoPei: PcdFdtBaseAddress=0, pas de HOB FDT publie\n"));
    }
  }

  return EFI_SUCCESS;
}
