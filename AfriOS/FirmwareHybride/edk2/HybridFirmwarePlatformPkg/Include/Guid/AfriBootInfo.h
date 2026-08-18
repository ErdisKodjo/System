/** @file
  GUIDs for AfriOS persistent NVRAM variables.

  Defines the vendor GUIDs under which the OTA/A-B-slot subsystems store
  their persistent state. These GUIDs are declared in the package .dec
  (HybridFirmwarePlatformPkg.dec, section [Guids]) and referenced by :

    - OtaUpdate/ABSlotManager.c    : gAfriBootInfoGuid (variable "AfriBootInfo")
    - OtaUpdate/FwUpdateAgent.c    : gAfriFwUpdateStateGuid (variable "AfriFwUpdateState")
    - Diagnostics/UefiShell/ShellExtensions.c : reads AfriBootInfo via gAfriBootInfoGuid

  Promoted from local definitions in those .c files (P2 — fix from the
  analysis report) so that any module that needs to read or write these
  variables uses the same GUID.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#ifndef AFRI_BOOT_INFO_GUID_H_
#define AFRI_BOOT_INFO_GUID_H_

//
// Forward declaration of the EFI_GUID type so this header can be parsed
// without including the full EDK2 MdePkg headers (useful for syntax
// checks outside an EDK2 build). In a real EDK2 build, <Uefi/UefiBaseType.h>
// is included transitively by <PiDxe.h> / <Uefi.h> before this header.
//
#ifndef __EFI_GUID_TYPEDEF__
typedef struct {
  unsigned int  Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char Data4[8];
} EFI_GUID;
#define __EFI_GUID_TYPEDEF__
#endif

//
// Vendor GUID for the "AfriBootInfo" NVRAM variable. Stores A/B slot
// state (active slot, boot success bitmask, retry count). Defined in
// HybridFirmwarePlatformPkg.dec [Guids].
//
extern EFI_GUID gAfriBootInfoGuid;

//
// Vendor GUID for the "AfriFwUpdateState" NVRAM variable. Stores the
// current firmware update state (Idle, Pending, Applying, Applied, Failed).
// Defined in HybridFirmwarePlatformPkg.dec [Guids].
//
extern EFI_GUID gAfriFwUpdateStateGuid;

#endif // AFRI_BOOT_INFO_GUID_H_
