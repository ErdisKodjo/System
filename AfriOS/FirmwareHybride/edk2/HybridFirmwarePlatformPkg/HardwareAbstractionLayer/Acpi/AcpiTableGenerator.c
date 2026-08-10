/** @file
  ACPI Table Generator DXE Driver.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformDetectLib.h>
#include <Protocol/AcpiTable.h>
#include <IndustryStandard/Acpi.h>

extern EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE Fadt;

/**
  Entry point for AcpiTableGenerator.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
AcpiTableGeneratorEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_ACPI_TABLE_PROTOCOL   *AcpiTableProtocol;
  UINTN                     TableKey;
  PLATFORM_BOOT_BACKEND     Backend;

  Backend = PlatformDetectBootBackend ();

  DEBUG ((DEBUG_INFO, "AcpiTableGenerator: Entry (backend detecte : %a)\n", PlatformBootBackendName (Backend)));

  if (Backend != PlatformBackendAcpi) {
    DEBUG ((DEBUG_INFO, "AcpiTableGenerator: backend != Acpi, aucune table installee sur cette plateforme "
                         "(voir FdtPlatformDxe pour le backend DeviceTree).\n"));
    return EFI_SUCCESS;
  }

  // 1. Locate the ACPI Table Protocol
  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AcpiTableGenerator: Failed to locate ACPI Table Protocol\n"));
    return Status;
  }

  // 2. Install FADT
  DEBUG ((DEBUG_INFO, "AcpiTableGenerator: Installing FADT\n"));
  TableKey = 0;
  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                &Fadt,
                                sizeof (Fadt),
                                &TableKey
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AcpiTableGenerator: Failed to install FADT\n"));
    return Status;
  }

  // 3. Install DSDT (In a real build, this would be loaded from a binary resource)
  DEBUG ((DEBUG_INFO, "AcpiTableGenerator: DSDT Installation (Stub for binary AML)\n"));

  return EFI_SUCCESS;
}
