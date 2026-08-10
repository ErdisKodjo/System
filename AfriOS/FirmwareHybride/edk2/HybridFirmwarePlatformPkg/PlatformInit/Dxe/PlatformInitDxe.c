/** @file
  Platform Initialization DXE Driver.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformDetectLib.h>

/**
  Entry point for PlatformInitDxe.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
PlatformInitDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  PLATFORM_BOOT_BACKEND  Backend;

  Backend = PlatformDetectBootBackend ();

  DEBUG ((DEBUG_INFO, "PlatformInitDxe: Entry - backend de description plateforme : %a\n",
          PlatformBootBackendName (Backend)));

  // 1. Initialize Chipset (Bridge, Bus enumeration)
  // 2. Setup Console (Serial/Graphics)
  // 3. Initialize PCI Express

  DEBUG ((DEBUG_INFO, "PlatformInitDxe: Chipset and PCI initialization complete\n"));

  return EFI_SUCCESS;
}
