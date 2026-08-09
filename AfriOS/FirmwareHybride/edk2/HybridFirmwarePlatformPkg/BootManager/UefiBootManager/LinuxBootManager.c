/** @file
  Linux Boot Manager for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

/**
  Finds and boots a Linux kernel.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
BootLinux (
  VOID
  )
{
  UINT8  PowerSource;

  DEBUG ((DEBUG_INFO, "LinuxBootManager: Searching for vmlinuz...\n"));

  PowerSource = PcdGet8 (PcdPowerSourceType);

  // In a real implementation, we would:
  // 1. Locate the kernel on the ESP or a dedicated partition.
  // 2. Prepare the kernel command line (e.g., adding "afros.power_source=solar").
  // 3. Load the initrd.
  // 4. Start the kernel.

  if (PowerSource == 2) {
    DEBUG ((DEBUG_INFO, "LinuxBootManager: Adding 'afros.power_source=solar' to kernel cmdline\n"));
  }

  DEBUG ((DEBUG_INFO, "LinuxBootManager: Linux kernel not found (Simulated)\n"));
  return EFI_NOT_FOUND;
}

/**
  Entry point for LinuxBootManager.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
LinuxBootManagerEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "LinuxBootManager: Entry\n"));
  return BootLinux ();
}
