/** @file
  A/B Slot Manager for Hybrid Firmware OTA Updates.

  Maintains a persistent NVRAM variable named "AfriBootInfo" containing the
  current A/B boot state:

    typedef struct {
      UINT8   ActiveSlot;       // 0 = slot A, 1 = slot B
      UINT8   BootSuccess;      // bitmask: bit0 = slot A ok, bit1 = slot B ok
      UINT8   RetryCount;       // remaining boot retries for active slot
      UINT8   Reserved[5];      // pad to 8 bytes for forward compatibility
    } AFRI_BOOT_INFO;

  Exposes:
    - AbGetActiveSlot()
    - AbSwitchSlot()
    - AbMarkBootSuccessful()
    - AbGetSlotState()

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define AFRI_BOOT_INFO_VAR_NAME   L"AfriBootInfo"
#define AFRI_BOOT_MAX_RETRIES     3

//
// Vendor GUID for the AfriBootInfo NVRAM variable.
//
EFI_GUID  gAfriBootInfoGuid = {
  0x8B6F2A1C, 0x3D5E, 0x4F8A, { 0xB1, 0xC2, 0xD3, 0xE4, 0xF5, 0xA6, 0xB7, 0xC8 }
};

typedef struct {
  UINT8   ActiveSlot;
  UINT8   BootSuccess;
  UINT8   RetryCount;
  UINT8   Reserved[5];
} AFRI_BOOT_INFO;

//
// Slot identifiers exposed to callers.
//
#define AB_SLOT_A   0
#define AB_SLOT_B   1

//
// Forward declaration: AbReadInfo() invokes AbWriteInfo() on first
// initialization, so the writer must be declared before the reader.
//
STATIC
EFI_STATUS
AbWriteInfo (
  IN AFRI_BOOT_INFO  *Info
  );

/**
  Reads the AfriBootInfo NVRAM variable. If the variable does not exist yet,
  initializes it with sensible defaults (slot A active, no success, full
  retries) and persists it.

  @param[out] Info   Pointer to a caller-allocated AFRI_BOOT_INFO buffer.

  @retval EFI_SUCCESS   Variable read (or initialized) successfully.
**/
STATIC
EFI_STATUS
AbReadInfo (
  OUT AFRI_BOOT_INFO  *Info
  )
{
  EFI_STATUS  Status;
  UINTN       Size;

  if (Info == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Size = sizeof (AFRI_BOOT_INFO);
  ZeroMem (Info, Size);

  Status = gRT->GetVariable (
                  AFRI_BOOT_INFO_VAR_NAME,
                  &gAfriBootInfoGuid,
                  NULL,
                  &Size,
                  Info
                  );
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "ABSlotManager: initializing default AfriBootInfo (slot A)\n"));
    Info->ActiveSlot  = AB_SLOT_A;
    Info->BootSuccess = 0;
    Info->RetryCount  = AFRI_BOOT_MAX_RETRIES;
    ZeroMem (Info->Reserved, sizeof (Info->Reserved));
    return AbWriteInfo (Info);
  }
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ABSlotManager: GetVariable failed (%r)\n", Status));
  }

  return Status;
}

/**
  Persists the AfriBootInfo NVRAM variable.

  @param[in] Info   Pointer to the AFRI_BOOT_INFO buffer to persist.

  @retval EFI_SUCCESS   Variable written successfully.
**/
STATIC
EFI_STATUS
AbWriteInfo (
  IN AFRI_BOOT_INFO  *Info
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  AFRI_BOOT_INFO_VAR_NAME,
                  &gAfriBootInfoGuid,
                  EFI_VARIABLE_NON_VOLATILE |
                  EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (AFRI_BOOT_INFO),
                  Info
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ABSlotManager: SetVariable failed (%r)\n", Status));
  }

  return Status;
}

/**
  Returns the currently active slot (0 = A, 1 = B).

  @param[out] Slot   Receives the active slot identifier.

  @retval EFI_SUCCESS   Slot read successfully.
**/
EFI_STATUS
EFIAPI
AbGetActiveSlot (
  OUT UINT8  *Slot
  )
{
  EFI_STATUS      Status;
  AFRI_BOOT_INFO  Info;

  if (Slot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AbReadInfo (&Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Slot = Info.ActiveSlot;
  DEBUG ((DEBUG_INFO, "ABSlotManager: active slot = %c\n",
          (Info.ActiveSlot == AB_SLOT_A) ? 'A' : 'B'));
  return EFI_SUCCESS;
}

/**
  Switches the active slot to the given value (0 = A, 1 = B) and resets the
  retry counter for the new slot.

  @param[in] Slot   The slot to make active (0 or 1).

  @retval EFI_SUCCESS           Slot switched.
  @retval EFI_INVALID_PARAMETER Slot is not 0 or 1.
**/
EFI_STATUS
EFIAPI
AbSwitchSlot (
  IN UINT8  Slot
  )
{
  EFI_STATUS      Status;
  AFRI_BOOT_INFO  Info;

  if (Slot > AB_SLOT_B) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AbReadInfo (&Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Info.ActiveSlot == Slot) {
    DEBUG ((DEBUG_INFO, "ABSlotManager: slot %c already active\n", (Slot == AB_SLOT_A) ? 'A' : 'B'));
    return EFI_SUCCESS;
  }

  Info.ActiveSlot = Slot;
  Info.RetryCount = AFRI_BOOT_MAX_RETRIES;
  Status = AbWriteInfo (&Info);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "ABSlotManager: switched active slot to %c\n",
            (Slot == AB_SLOT_A) ? 'A' : 'B'));
  }
  return Status;
}

/**
  Marks the currently active slot as having booted successfully, clearing the
  retry counter and setting the corresponding bit in BootSuccess.

  @retval EFI_SUCCESS   Active slot marked successful.
**/
EFI_STATUS
EFIAPI
AbMarkBootSuccessful (
  VOID
  )
{
  EFI_STATUS      Status;
  AFRI_BOOT_INFO  Info;
  UINT8           SuccessBit;

  Status = AbReadInfo (&Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SuccessBit = (UINT8)((Info.ActiveSlot == AB_SLOT_A) ? BIT0 : BIT1);
  if ((Info.BootSuccess & SuccessBit) != 0) {
    DEBUG ((DEBUG_INFO, "ABSlotManager: slot %c already marked successful\n",
            (Info.ActiveSlot == AB_SLOT_A) ? 'A' : 'B'));
    return EFI_SUCCESS;
  }

  Info.BootSuccess = (UINT8)(Info.BootSuccess | SuccessBit);
  Info.RetryCount  = 0;
  Status = AbWriteInfo (&Info);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "ABSlotManager: marked slot %c successful\n",
            (Info.ActiveSlot == AB_SLOT_A) ? 'A' : 'B'));
  }
  return Status;
}

/**
  Returns the state of the given slot.

  @param[in]  Slot       The slot identifier (0 or 1).
  @param[out] IsActive   Receives TRUE if the slot is the active one.
  @param[out] IsGood     Receives TRUE if the slot is marked boot-successful.
  @param[out] RetryLeft  Receives the remaining retry count for the slot.

  @retval EFI_SUCCESS             Slot state returned.
  @retval EFI_INVALID_PARAMETER   Bad slot or null pointer.
**/
EFI_STATUS
EFIAPI
AbGetSlotState (
  IN  UINT8    Slot,
  OUT BOOLEAN  *IsActive,
  OUT BOOLEAN  *IsGood,
  OUT UINT8    *RetryLeft
  )
{
  EFI_STATUS      Status;
  AFRI_BOOT_INFO  Info;
  UINT8           SuccessBit;

  if (Slot > AB_SLOT_B) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AbReadInfo (&Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SuccessBit = (UINT8)((Slot == AB_SLOT_A) ? BIT0 : BIT1);

  if (IsActive != NULL) {
    *IsActive = (BOOLEAN)(Info.ActiveSlot == Slot);
  }
  if (IsGood != NULL) {
    *IsGood = (BOOLEAN)((Info.BootSuccess & SuccessBit) != 0);
  }
  if (RetryLeft != NULL) {
    *RetryLeft = (Info.ActiveSlot == Slot) ? Info.RetryCount : 0;
  }

  return EFI_SUCCESS;
}

/**
  Decrements the retry counter for the active slot. If the counter reaches
  zero, the active slot is switched to the other slot (fallback).

  @retval EFI_SUCCESS   Retry counter updated.
**/
EFI_STATUS
EFIAPI
AbDecrementRetry (
  VOID
  )
{
  EFI_STATUS      Status;
  AFRI_BOOT_INFO  Info;

  Status = AbReadInfo (&Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Info.RetryCount == 0) {
    DEBUG ((DEBUG_WARN, "ABSlotManager: retries exhausted, switching slot\n"));
    return AbSwitchSlot ((UINT8)(Info.ActiveSlot ^ 1));
  }

  Info.RetryCount = (UINT8)(Info.RetryCount - 1);
  Status = AbWriteInfo (&Info);
  DEBUG ((DEBUG_INFO, "ABSlotManager: retries left = %u\n", (UINT32)Info.RetryCount));
  return Status;
}

/**
  Entry point for the A/B Slot Manager module. Reads the current state once at
  load time to make sure the NVRAM variable is initialized.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
ABSlotManagerEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS      Status;
  AFRI_BOOT_INFO  Info;

  Status = AbReadInfo (&Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_INFO, "ABSlotManager: ready (active=%c, success=0x%02x, retries=%u)\n",
          (Info.ActiveSlot == AB_SLOT_A) ? 'A' : 'B',
          (UINT32)Info.BootSuccess,
          (UINT32)Info.RetryCount));
  return EFI_SUCCESS;
}
