/** @file
  Firmware Update Agent for Hybrid Firmware OTA Updates.

  Reads a UEFI Capsule from disk (\EFI\AfriOS\Updates\update.cap), validates
  its EFI_CAPSULE_HEADER (Guid, Flags, ImageSize), then calls
  gBS->UpdateCapsule() for processing at next reboot.

  Exposes:
    - FwUpdateCheck()  : checks if a capsule is present on disk.
    - FwUpdateApply()  : loads, validates and submits the capsule.
    - FwUpdateStatus() : returns the current update state from NVRAM.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/Capsule.h>
#include <Guid/CapsuleVendor.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>

#define FW_UPDATE_CAP_PATH       L"\\EFI\\AfriOS\\Updates\\update.cap"
#define FW_UPDATE_STATE_VAR      L"AfriFwUpdateState"
#define FW_UPDATE_MAX_CAPSULES   4

//
// Vendor GUID under which the FwUpdateState NVRAM variable is stored.
//
EFI_GUID  gAfriFwUpdateStateGuid = {
  0x4D5E6F70, 0x8192, 0x4A3B, { 0xB4, 0xC5, 0xD6, 0xE7, 0xF8, 0x09, 0x1A, 0x2B }
};

//
// Persistent update states.
//
typedef enum {
  FwUpdateStateIdle = 0,
  FwUpdateStatePending,
  FwUpdateStateApplying,
  FwUpdateStateApplied,
  FwUpdateStateFailed
} FW_UPDATE_STATE;

//
// GUID we accept for the AfriOS firmware capsule.
//
STATIC EFI_GUID  mAfriCapsuleGuid = {
  0x3B8D2E1F, 0x4C5A, 0x6D7E, { 0x8F, 0x90, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 }
};

/**
  Reads the entire content of a file on the ESP into an allocated buffer.

  @param[in]  Fs        Pointer to the SimpleFileSystem protocol.
  @param[in]  Path      UTF-16 path of the file.
  @param[out] Buffer    Receives a pointer to the allocated buffer.
  @param[out] Size      Receives the size in bytes.

  @retval EFI_SUCCESS   File read successfully.
**/
STATIC
EFI_STATUS
ReadCapFile (
  IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *Fs,
  IN  CHAR16                           *Path,
  OUT VOID                             **Buffer,
  OUT UINTN                            *Size
  )
{
  EFI_FILE_PROTOCOL  *Root;
  EFI_FILE_PROTOCOL  *File;
  EFI_STATUS         Status;
  UINTN              FileInfoSize;
  EFI_FILE_INFO      *FileInfo;
  UINTN              ReadSize;

  Status = Fs->OpenVolume (Fs, &Root);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Root->Open (Root, &File, Path, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Root->Close (Root);
    return Status;
  }

  FileInfoSize = 0;
  FileInfo     = NULL;
  Status = File->GetInfo (File, &gEfiFileInfoGuid, &FileInfoSize, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    FileInfo = AllocatePool (FileInfoSize);
    Status = File->GetInfo (File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
  }
  if (EFI_ERROR (Status) || (FileInfo == NULL)) {
    File->Close (File);
    Root->Close (Root);
    return Status;
  }

  *Size   = (UINTN)FileInfo->FileSize;
  *Buffer = AllocatePool (*Size);
  FreePool (FileInfo);
  if (*Buffer == NULL) {
    File->Close (File);
    Root->Close (Root);
    return EFI_OUT_OF_RESOURCES;
  }

  ReadSize = *Size;
  Status = File->Read (File, &ReadSize, *Buffer);
  File->Close (File);
  Root->Close (Root);
  if (EFI_ERROR (Status) || (ReadSize != *Size)) {
    FreePool (*Buffer);
    *Buffer = NULL;
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Validates the EFI_CAPSULE_HEADER of a candidate capsule.

  @param[in] Capsule   Pointer to the capsule in memory.
  @param[in] Size      Total size of the capsule buffer.

  @retval EFI_SUCCESS           Capsule header is valid.
  @retval EFI_INVALID_PARAMETER Bad pointer or zero size.
  @retval EFI_VOLUME_CORRUPTED  Header fields inconsistent.
**/
STATIC
EFI_STATUS
ValidateCapsuleHeader (
  IN EFI_CAPSULE_HEADER  *Capsule,
  IN UINTN               Size
  )
{
  if ((Capsule == NULL) || (Size < sizeof (EFI_CAPSULE_HEADER))) {
    DEBUG ((DEBUG_ERROR, "FwUpdateAgent: capsule too small (%lu bytes)\n", (UINT64)Size));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Accept either the AfriOS firmware capsule GUID or the standard
  // capsule-on-disk GUID.
  //
  if ((CompareGuid (&Capsule->CapsuleGuid, &mAfriCapsuleGuid)) &&
      (CompareGuid (&Capsule->CapsuleGuid, &gEfiCapsuleVendorGuid))) {
    DEBUG ((DEBUG_ERROR, "FwUpdateAgent: capsule GUID mismatch\n"));
    return EFI_VOLUME_CORRUPTED;
  }

  if (Capsule->HeaderSize < sizeof (EFI_CAPSULE_HEADER)) {
    DEBUG ((DEBUG_ERROR, "FwUpdateAgent: HeaderSize too small (%u)\n", (UINT32)Capsule->HeaderSize));
    return EFI_VOLUME_CORRUPTED;
  }

  if (Capsule->ImageSize > Size) {
    DEBUG ((DEBUG_ERROR, "FwUpdateAgent: ImageSize (%lu) > buffer size (%lu)\n",
            (UINT64)Capsule->ImageSize, (UINT64)Size));
    return EFI_VOLUME_CORRUPTED;
  }

  if ((Capsule->Flags & CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE) != 0) {
    DEBUG ((DEBUG_INFO, "FwUpdateAgent: capsule populates system table\n"));
  }
  if ((Capsule->Flags & CAPSULE_FLAGS_PERSIST_ACROSS_RESET) == 0) {
    DEBUG ((DEBUG_WARN, "FwUpdateAgent: capsule does not persist across reset\n"));
  }

  DEBUG ((DEBUG_INFO,
          "FwUpdateAgent: capsule valid (Guid=%g, HeaderSize=%u, ImageSize=%lu, Flags=0x%08x)\n",
          &Capsule->CapsuleGuid, (UINT32)Capsule->HeaderSize,
          (UINT64)Capsule->ImageSize, (UINT32)Capsule->Flags));

  return EFI_SUCCESS;
}

/**
  Persists the current update state in NVRAM.

  @param[in] State   The new update state.

  @retval EFI_SUCCESS   State persisted.
**/
STATIC
EFI_STATUS
FwUpdateSetState (
  IN FW_UPDATE_STATE  State
  )
{
  UINT8  Value;

  Value = (UINT8)State;
  return gRT->SetVariable (
                FW_UPDATE_STATE_VAR,
                &gAfriFwUpdateStateGuid,
                EFI_VARIABLE_NON_VOLATILE |
                EFI_VARIABLE_BOOTSERVICE_ACCESS |
                EFI_VARIABLE_RUNTIME_ACCESS,
                sizeof (Value),
                &Value
                );
}

/**
  Checks if a firmware update capsule is present on disk.

  @param[out] CapsuleSize   If non-NULL, receives the size of the capsule.

  @retval EFI_SUCCESS       A capsule is present and ready to apply.
  @retval EFI_NOT_FOUND     No capsule present.
**/
EFI_STATUS
EFIAPI
FwUpdateCheck (
  OUT UINTN  *CapsuleSize OPTIONAL
  )
{
  EFI_STATUS                          Status;
  UINTN                               HandleCount;
  EFI_HANDLE                          *HandleBuffer;
  UINTN                               Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *Fs;
  EFI_FILE_PROTOCOL                   *Root;
  EFI_FILE_PROTOCOL                   *File;
  EFI_FILE_INFO                       *Info;
  UINTN                               InfoSize;

  DEBUG ((DEBUG_INFO, "FwUpdateAgent: checking for %s\n", FW_UPDATE_CAP_PATH));

  HandleCount  = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    return EFI_NOT_FOUND;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&Fs
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }
    Status = Root->Open (Root, &File, FW_UPDATE_CAP_PATH, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR (Status)) {
      if (CapsuleSize != NULL) {
        InfoSize = 0;
        Info     = NULL;
        if (File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, NULL) == EFI_BUFFER_TOO_SMALL) {
          Info = AllocatePool (InfoSize);
          if (Info != NULL) {
            File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, Info);
            *CapsuleSize = (UINTN)Info->FileSize;
            FreePool (Info);
          }
        }
      }
      File->Close (File);
      Root->Close (Root);
      gBS->FreePool (HandleBuffer);
      DEBUG ((DEBUG_INFO, "FwUpdateAgent: capsule present\n"));
      return EFI_SUCCESS;
    }
    Root->Close (Root);
  }

  gBS->FreePool (HandleBuffer);
  return EFI_NOT_FOUND;
}

/**
  Applies a pending firmware update: loads the capsule, validates it, and
  submits it via gBS->UpdateCapsule() for processing at next reboot.

  @retval EFI_SUCCESS   Capsule submitted, system will process it on reboot.
**/
EFI_STATUS
EFIAPI
FwUpdateApply (
  VOID
  )
{
  EFI_STATUS                          Status;
  UINTN                               HandleCount;
  EFI_HANDLE                          *HandleBuffer;
  UINTN                               Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *Fs;
  EFI_HANDLE                          EspHandle;
  VOID                                *CapsuleBuffer;
  UINTN                               CapsuleSize;
  EFI_CAPSULE_HEADER                  *CapsuleHeaders[FW_UPDATE_MAX_CAPSULES];
  EFI_CAPSULE_BLOCK_DESCRIPTOR        *ScatterGatherList;

  DEBUG ((DEBUG_INFO, "FwUpdateAgent: applying update\n"));

  FwUpdateSetState (FwUpdateStateApplying);

  EspHandle    = NULL;
  HandleCount  = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    FwUpdateSetState (FwUpdateStateFailed);
    return EFI_NOT_FOUND;
  }

  CapsuleBuffer = NULL;
  CapsuleSize   = 0;
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&Fs
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }
    Status = ReadCapFile (Fs, FW_UPDATE_CAP_PATH, &CapsuleBuffer, &CapsuleSize);
    if (!EFI_ERROR (Status)) {
      EspHandle = HandleBuffer[Index];
      break;
    }
  }
  gBS->FreePool (HandleBuffer);

  if ((CapsuleBuffer == NULL) || (CapsuleSize == 0)) {
    DEBUG ((DEBUG_ERROR, "FwUpdateAgent: capsule not found\n"));
    FwUpdateSetState (FwUpdateStateFailed);
    return EFI_NOT_FOUND;
  }

  Status = ValidateCapsuleHeader ((EFI_CAPSULE_HEADER *)CapsuleBuffer, CapsuleSize);
  if (EFI_ERROR (Status)) {
    FreePool (CapsuleBuffer);
    FwUpdateSetState (FwUpdateStateFailed);
    return Status;
  }

  //
  // Build a one-entry scatter/gather list pointing at the capsule buffer.
  // The buffer must remain valid across the subsequent reboot, so the caller
  // (firmware) is responsible for keeping it pinned in EfiReservedMemory.
  //
  ScatterGatherList = AllocatePool (2 * sizeof (EFI_CAPSULE_BLOCK_DESCRIPTOR));
  if (ScatterGatherList == NULL) {
    FreePool (CapsuleBuffer);
    FwUpdateSetState (FwUpdateStateFailed);
    return EFI_OUT_OF_RESOURCES;
  }
  ScatterGatherList[0].Length   = CapsuleSize;
  ScatterGatherList[0].Data     = (EFI_PHYSICAL_ADDRESS)(UINTN)CapsuleBuffer;
  ScatterGatherList[1].Length   = 0;
  ScatterGatherList[1].Data     = 0;

  CapsuleHeaders[0] = (EFI_CAPSULE_HEADER *)CapsuleBuffer;

  Status = gBS->UpdateCapsule (
                  CapsuleHeaders,
                  1,
                  (EFI_PHYSICAL_ADDRESS)(UINTN)ScatterGatherList
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "FwUpdateAgent: UpdateCapsule failed (%r)\n", Status));
    FreePool (ScatterGatherList);
    FreePool (CapsuleBuffer);
    FwUpdateSetState (FwUpdateStateFailed);
    return Status;
  }

  FwUpdateSetState (FwUpdateStatePending);
  DEBUG ((DEBUG_INFO, "FwUpdateAgent: capsule submitted, will apply on next reboot\n"));
  return EFI_SUCCESS;
}

/**
  Returns the current firmware update state from NVRAM.

  @param[out] State   Receives the current update state.

  @retval EFI_SUCCESS   State read successfully.
**/
EFI_STATUS
EFIAPI
FwUpdateStatus (
  OUT FW_UPDATE_STATE  *State
  )
{
  EFI_STATUS  Status;
  UINTN       Size;
  UINT8       Value;

  if (State == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Value = (UINT8)FwUpdateStateIdle;
  Size  = sizeof (Value);
  Status = gRT->GetVariable (FW_UPDATE_STATE_VAR, &gAfriFwUpdateStateGuid, NULL, &Size, &Value);
  if (Status == EFI_NOT_FOUND) {
    *State = FwUpdateStateIdle;
    return EFI_SUCCESS;
  }
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *State = (FW_UPDATE_STATE)Value;
  return EFI_SUCCESS;
}

/**
  Entry point for the Firmware Update Agent.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
FwUpdateAgentEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  FW_UPDATE_STATE  State;

  if (!EFI_ERROR (FwUpdateStatus (&State))) {
    DEBUG ((DEBUG_INFO, "FwUpdateAgent: ready (state=%u)\n", (UINT32)State));
  } else {
    DEBUG ((DEBUG_INFO, "FwUpdateAgent: ready (state=uninitialized)\n"));
  }
  return EFI_SUCCESS;
}
