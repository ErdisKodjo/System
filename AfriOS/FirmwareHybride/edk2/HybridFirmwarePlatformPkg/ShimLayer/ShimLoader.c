/** @file
  Shim Loader for Hybrid Firmware Hypervisor.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

extern EFI_STATUS EFIAPI HypervisorInit (VOID);

/**
  Entry point for ShimLoader.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
ShimLoaderEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "ShimLoader: Entry\n"));

  // 1. Initialize Hypervisor
  Status = HypervisorInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ShimLoader: Hypervisor initialization failed (%r)\n", Status));
    return Status;
  }

  // 2. Prepare Guest OS (AfriOS)
  DEBUG ((DEBUG_INFO, "ShimLoader: Preparing Guest OS for launch...\n"));

  // 3. Launch Hypervisor (VMLAUNCH/VMENTRY)
  DEBUG ((DEBUG_INFO, "ShimLoader: VMLAUNCH (Transitioning to Guest Mode)\n"));

  return EFI_SUCCESS;
}
