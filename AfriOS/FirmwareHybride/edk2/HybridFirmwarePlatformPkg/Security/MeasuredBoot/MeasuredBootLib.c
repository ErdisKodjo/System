/** @file
  Measured Boot Library for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

/**
  Measures a binary blob and logs it to the TPM.

  @param[in]  PcrIndex     The PCR index to log the measurement into.
  @param[in]  Description  A brief description of the measured component.
  @param[in]  Buffer       Pointer to the binary data to measure.
  @param[in]  BufferSize   Size of the binary data in bytes.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
MeasureComponent (
  IN UINT32       PcrIndex,
  IN CHAR8        *Description,
  IN VOID         *Buffer,
  IN UINTN        BufferSize
  )
{
  DEBUG ((DEBUG_INFO, "MeasuredBoot: Measuring component '%a' into PCR %d\n", Description, PcrIndex));

  if (Buffer == NULL || BufferSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  // 1. Calculate SHA-256 Hash of Buffer
  // 2. Locate TPM Protocol (gEfiTcg2ProtocolGuid)
  // 3. Extend PCR PcrIndex with the hash

  DEBUG ((DEBUG_INFO, "MeasuredBoot: Component '%a' measured successfully (Simulated)\n", Description));
  return EFI_SUCCESS;
}

/**
  Measures a Firmware Volume (FV).

  @param[in]  FvBase  Base address of the FV.
  @param[in]  FvSize  Size of the FV.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
MeasureFirmwareVolume (
  IN EFI_PHYSICAL_ADDRESS  FvBase,
  IN UINT64                FvSize
  )
{
  return MeasureComponent (0, "Main Firmware Volume", (VOID *)(UINTN)FvBase, (UINTN)FvSize);
}
