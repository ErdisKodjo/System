/** @file
  Device Tree Platform DXE Driver.

  Installe le blob Device Tree comme table de configuration UEFI quand
  PlatformDetectLib a choisi le backend DeviceTree. Symétrique
  d'AcpiTableGenerator.c pour le backend Acpi — voir docs/architecture_overview.md.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformDetectLib.h>

/**
  Récupère l'adresse du blob FDT publiée par PlatformInfoPei (HOB
  gHybridFirmwareFdtBlobHobGuid, voir PlatformInit/Pei/PlatformInfoPei.c).

  @param[out] FdtBase  Adresse du blob si trouvée.

  @return EFI_SUCCESS si le HOB existe et a la taille attendue,
          EFI_NOT_FOUND sinon (PcdFdtBaseAddress valait 0 en phase PEI —
          carte non configurée pour le backend DeviceTree).
**/
STATIC
EFI_STATUS
GetPlatformFdtBlobBase (
  OUT UINT64  *FdtBase
  )
{
  VOID  *HobData;
  UINTN HobDataSize;

  HobData = GetFirstGuidHob (&gHybridFirmwareFdtBlobHobGuid);
  if (HobData == NULL) {
    return EFI_NOT_FOUND;
  }

  HobDataSize = GET_GUID_HOB_DATA_SIZE (HobData);
  if (HobDataSize != sizeof (UINT64)) {
    DEBUG ((DEBUG_ERROR, "FdtPlatformDxe: HOB FDT de taille inattendue (%d octets)\n", HobDataSize));
    return EFI_NOT_FOUND;
  }

  CopyMem (FdtBase, GET_GUID_HOB_DATA (HobData), sizeof (UINT64));
  return EFI_SUCCESS;
}

/**
  Entry point for FdtPlatformDxe.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if the backend isn't DeviceTree (no-op) or if the FDT
          configuration table was installed successfully.
**/
EFI_STATUS
EFIAPI
FdtPlatformDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS              Status;
  PLATFORM_BOOT_BACKEND   Backend;
  UINT64                  FdtBase;

  Backend = PlatformDetectBootBackend ();

  DEBUG ((DEBUG_INFO, "FdtPlatformDxe: Entry (backend detecte : %a)\n", PlatformBootBackendName (Backend)));

  if (Backend != PlatformBackendDeviceTree) {
    DEBUG ((DEBUG_INFO, "FdtPlatformDxe: backend != DeviceTree, ce driver ne fait rien sur cette plateforme.\n"));
    return EFI_SUCCESS;
  }

  Status = GetPlatformFdtBlobBase (&FdtBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "FdtPlatformDxe: backend DeviceTree mais aucun blob FDT fourni par la plateforme "
                          "(HOB gHybridFirmwareFdtBlobHobGuid absent - PcdFdtBaseAddress non configure pour cette carte)\n"));
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "FdtPlatformDxe: installation du blob FDT a 0x%lx comme table de configuration\n", FdtBase));

  Status = gBS->InstallConfigurationTable (&gHybridFirmwareFdtTableGuid, (VOID *)(UINTN)FdtBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "FdtPlatformDxe: InstallConfigurationTable a echoue : %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}
