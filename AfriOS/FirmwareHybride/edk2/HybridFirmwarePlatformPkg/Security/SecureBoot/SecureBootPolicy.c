/** @file
  Secure Boot Policy Manager for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Guid/GlobalVariable.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/ImageAuthentication.h>

/**
  Checks the current Secure Boot status.

  @return TRUE if Secure Boot is enabled and active.
**/
BOOLEAN
IsSecureBootActive (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT8       SecureBoot;
  UINTN       Size;

  Size = sizeof (SecureBoot);
  Status = gRT->GetVariable (
                  EFI_SECURE_BOOT_MODE_NAME,
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &Size,
                  &SecureBoot
                  );

  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return (SecureBoot == 1);
}

/**
  Entry point for SecureBootPolicy.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
SecureBootPolicyEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "SecureBootPolicy: Entry\n"));

  if (IsSecureBootActive ()) {
    DEBUG ((DEBUG_INFO, "SecureBootPolicy: SECURE BOOT IS ACTIVE (Enforcing Policy)\n"));
  } else {
    DEBUG ((DEBUG_WARN, "SecureBootPolicy: Secure Boot is disabled or not configured\n"));
    // Optionally: Auto-enroll default keys if in "Setup Mode"
  }

  return EFI_SUCCESS;
}
