/** @file
  ACPI Platform DXE Driver.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/AcpiTable.h>

/**
  Entry point for AcpiPlatformDxe.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
AcpiPlatformEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "AcpiPlatformDxe: Entry\n"));

  // This driver typically handles platform-specific ACPI logic
  // and ensures the AcpiTableGenerator has what it needs.

  return EFI_SUCCESS;
}
