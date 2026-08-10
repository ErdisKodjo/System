/** @file
  SMBIOS Table Generator for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Protocol/Smbios.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <IndustryStandard/SmBios.h>

/**
  Install SMBIOS Type 0 (BIOS Information).

  @param[in] Smbios  Pointer to SMBIOS protocol.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
InstallSmbiosType0 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  EFI_STATUS            Status;
  SMBIOS_TABLE_TYPE0    *Type0Record;
  EFI_SMBIOS_HANDLE     SmbiosHandle;
  CHAR8                 *OptionalStrings;

  // Allocate record with room for strings
  Type0Record = AllocateZeroPool (sizeof (SMBIOS_TABLE_TYPE0) + 100);
  if (Type0Record == NULL) return EFI_OUT_OF_RESOURCES;

  Type0Record->Hdr.Type = 0;
  Type0Record->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE0);
  Type0Record->Vendor = 1; // 1st string
  Type0Record->BiosVersion = 2; // 2nd string
  Type0Record->BiosReleaseDate = 3; // 3rd string
  Type0Record->BiosSegment = 0xF000;
  Type0Record->BiosCharacteristics.BiosCharacteristicsNotSupported = 0;

  OptionalStrings = (CHAR8 *)(Type0Record + 1);
  AsciiStrCpyS (OptionalStrings, 100, "AfriOS Foundation");
  AsciiStrCpyS (OptionalStrings + AsciiStrLen(OptionalStrings) + 1, 100 - AsciiStrLen(OptionalStrings) - 1, "v1.0-Hybrid");
  AsciiStrCpyS (OptionalStrings + AsciiStrLen(OptionalStrings) + 1 + AsciiStrLen(OptionalStrings + AsciiStrLen(OptionalStrings) + 1) + 1, 100, "05/13/2026");

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = Smbios->Add (Smbios, NULL, &SmbiosHandle, (EFI_SMBIOS_TABLE_HEADER *)Type0Record);

  FreePool (Type0Record);
  return Status;
}

/**
  Install SMBIOS Type 1 (System Information).

  @param[in] Smbios  Pointer to SMBIOS protocol.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
InstallSmbiosType1 (
  IN EFI_SMBIOS_PROTOCOL  *Smbios
  )
{
  EFI_STATUS            Status;
  SMBIOS_TABLE_TYPE1    *Type1Record;
  EFI_SMBIOS_HANDLE     SmbiosHandle;
  CHAR8                 *OptionalStrings;

  Type1Record = AllocateZeroPool (sizeof (SMBIOS_TABLE_TYPE1) + 100);
  if (Type1Record == NULL) return EFI_OUT_OF_RESOURCES;

  Type1Record->Hdr.Type = 1;
  Type1Record->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE1);
  Type1Record->Manufacturer = 1;
  Type1Record->ProductName = 2;
  Type1Record->Version = 3;
  Type1Record->WakeUpType = SystemWakeupTypePowerSwitch;

  OptionalStrings = (CHAR8 *)(Type1Record + 1);
  AsciiStrCpyS (OptionalStrings, 100, "AfriOS");
  AsciiStrCpyS (OptionalStrings + AsciiStrLen(OptionalStrings) + 1, 100, "Hybrid Computing Platform");
  AsciiStrCpyS (OptionalStrings + AsciiStrLen(OptionalStrings) + 1 + AsciiStrLen(OptionalStrings + AsciiStrLen(OptionalStrings) + 1) + 1, 100, "1.0");

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = Smbios->Add (Smbios, NULL, &SmbiosHandle, (EFI_SMBIOS_TABLE_HEADER *)Type1Record);

  FreePool (Type1Record);
  return Status;
}

/**
  Entry point for SmbiosGenerator.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
SmbiosGeneratorEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS              Status;
  EFI_SMBIOS_PROTOCOL     *Smbios;

  DEBUG ((DEBUG_INFO, "SmbiosGenerator: Entry\n"));

  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SmbiosGenerator: Could not locate SMBIOS protocol\n"));
    return Status;
  }

  InstallSmbiosType0 (Smbios);
  InstallSmbiosType1 (Smbios);
  // Type 4 and 17 would follow similar patterns

  return EFI_SUCCESS;
}
