/** @file
  Windows Boot Manager for Hybrid Firmware.

  This module locates the Windows boot manager (\EFI\Microsoft\Boot\bootmgfw.efi)
  on the ESP, optionally parses a minimal subset of the BCD (Boot Configuration
  Data) stored in \EFI\Microsoft\Boot\BCD, then loads and starts the Windows
  boot manager via gBS->LoadImage / gBS->StartImage.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>

//
// Path to the Windows boot manager on the EFI System Partition.
//
#define WINDOWS_BOOTMGR_PATH    L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi"
#define WINDOWS_BCD_PATH        L"\\EFI\\Microsoft\\Boot\\BCD"

//
// Minimal BCD object GUID we look for (bootmgr application). The Windows BCD
// format is a registry hive; we only parse enough to detect the default boot
// entry identifier. Full BCD parsing is delegated to bootmgfw.efi itself.
//
#define BCD_BOOTMGR_OBJ_OFFSET  0x1000

/**
  Locates the EFI System Partition handle that contains a Windows boot manager.

  @param[out]  FsHandle   Pointer to the file system handle on success.

  @retval EFI_SUCCESS     ESP located successfully.
  @retval EFI_NOT_FOUND   No ESP contains the Windows boot manager.
**/
STATIC
EFI_STATUS
LocateWindowsEsp (
  OUT EFI_HANDLE  *FsHandle
  )
{
  EFI_STATUS                          Status;
  UINTN                               HandleCount;
  EFI_HANDLE                          *HandleBuffer;
  UINTN                               Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *Fs;
  EFI_FILE_PROTOCOL                   *Root;
  EFI_FILE_PROTOCOL                   *BootMgrFile;

  DEBUG ((DEBUG_INFO, "WinBootMgr: Scanning handles for SimpleFileSystem...\n"));

  HandleCount   = 0;
  HandleBuffer  = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    DEBUG ((DEBUG_ERROR, "WinBootMgr: No SimpleFileSystem handles found (%r)\n", Status));
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

    Status = Root->Open (Root, &BootMgrFile, WINDOWS_BOOTMGR_PATH, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR (Status)) {
      BootMgrFile->Close (BootMgrFile);
      Root->Close (Root);
      *FsHandle = HandleBuffer[Index];
      gBS->FreePool (HandleBuffer);
      DEBUG ((DEBUG_INFO, "WinBootMgr: Found %s on handle %p\n", WINDOWS_BOOTMGR_PATH, *FsHandle));
      return EFI_SUCCESS;
    }

    Root->Close (Root);
  }

  gBS->FreePool (HandleBuffer);
  DEBUG ((DEBUG_ERROR, "WinBootMgr: bootmgfw.efi not found on any ESP\n"));
  return EFI_NOT_FOUND;
}

/**
  Reads the BCD registry hive and extracts the default boot entry identifier.

  This is a minimal stub: it reads the first BCD_BOOTMGR_OBJ_OFFSET bytes,
  validates the "regf" hive signature and returns the first object identifier
  it finds. Full BCD parsing is performed by bootmgfw.efi at runtime.

  @param[in]  FsHandle      File system handle to the ESP.
  @param[out] DefaultEntry  Pointer to receive the default boot entry id.

  @retval EFI_SUCCESS       BCD parsed (or stubbed) successfully.
**/
STATIC
EFI_STATUS
ParseBcdDefaultEntry (
  IN  EFI_HANDLE  FsHandle,
  OUT UINT32      *DefaultEntry
  )
{
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Fs;
  EFI_FILE_PROTOCOL                 *Root;
  EFI_FILE_PROTOCOL                 *BcdFile;
  UINT8                             HiveHeader[16];
  UINTN                             ReadSize;
  EFI_STATUS                        Status;

  DEBUG ((DEBUG_INFO, "WinBootMgr: Parsing BCD hive at %s\n", WINDOWS_BCD_PATH));

  Status = gBS->HandleProtocol (FsHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Fs->OpenVolume (Fs, &Root);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Root->Open (Root, &BcdFile, WINDOWS_BCD_PATH, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "WinBootMgr: BCD not found (%r), using default entry 0x01\n", Status));
    Root->Close (Root);
    *DefaultEntry = 0x01;
    return EFI_SUCCESS;
  }

  ReadSize = sizeof (HiveHeader);
  Status = BcdFile->Read (BcdFile, &ReadSize, HiveHeader);
  BcdFile->Close (BcdFile);
  Root->Close (Root);

  if (EFI_ERROR (Status) || (ReadSize < 4)) {
    DEBUG ((DEBUG_WARN, "WinBootMgr: BCD read failed (%r), defaulting\n", Status));
    *DefaultEntry = 0x01;
    return EFI_SUCCESS;
  }

  if (CompareMem (HiveHeader, "regf", 4) != 0) {
    DEBUG ((DEBUG_WARN, "WinBootMgr: BCD signature mismatch, defaulting\n"));
    *DefaultEntry = 0x01;
    return EFI_SUCCESS;
  }

  //
  // In a real implementation, we would walk the regf hive cells to find the
  // {bootmgr} object and its "default" element. For the stub, we always
  // return entry 0x01 (Windows Boot Loader).
  //
  *DefaultEntry = 0x01;
  return EFI_SUCCESS;
}

/**
  Builds a device path for the Windows boot manager on the given ESP handle.

  @param[in]   FsHandle   The ESP file system handle.
  @param[out]  DevicePath Pointer to the allocated device path.

  @retval EFI_SUCCESS     Device path created.
**/
STATIC
EFI_STATUS
BuildBootMgrDevicePath (
  IN  EFI_HANDLE          FsHandle,
  OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *FsDp;
  EFI_DEVICE_PATH_PROTOCOL  *FullDp;
  EFI_STATUS                Status;

  Status = gBS->HandleProtocol (FsHandle, &gEfiDevicePathProtocolGuid, (VOID **)&FsDp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FullDp = FileDevicePath (FsHandle, WINDOWS_BOOTMGR_PATH);
  if (FullDp == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *DevicePath = FullDp;
  return EFI_SUCCESS;
}

/**
  Locates, loads and starts the Windows Boot Manager (bootmgfw.efi).

  @return EFI_SUCCESS if the Windows boot manager was started.
**/
EFI_STATUS
EFIAPI
BootWindows (
  VOID
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                FsHandle;
  EFI_HANDLE                BootMgrImage;
  EFI_DEVICE_PATH_PROTOCOL  *BootMgrDp;
  UINT32                    DefaultEntry;
  UINTN                     ExitDataSize;
  CHAR16                    *ExitData;

  DEBUG ((DEBUG_INFO, "WinBootMgr: Entry\n"));

  FsHandle = NULL;
  Status = LocateWindowsEsp (&FsHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ParseBcdDefaultEntry (FsHandle, &DefaultEntry);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "WinBootMgr: BCD parse failed (%r), continuing\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "WinBootMgr: Default BCD entry = 0x%08x\n", DefaultEntry));
  }

  BootMgrDp = NULL;
  Status = BuildBootMgrDevicePath (FsHandle, &BootMgrDp);
  if (EFI_ERROR (Status) || (BootMgrDp == NULL)) {
    DEBUG ((DEBUG_ERROR, "WinBootMgr: Failed to build device path (%r)\n", Status));
    return Status;
  }

  BootMgrImage = NULL;
  Status = gBS->LoadImage (
                  FALSE,
                  gImageHandle,
                  BootMgrDp,
                  NULL,
                  0,
                  &BootMgrImage
                  );
  gBS->FreePool (BootMgrDp);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WinBootMgr: LoadImage failed (%r)\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "WinBootMgr: Starting bootmgfw.efi (image=%p)\n", BootMgrImage));

  ExitData     = NULL;
  ExitDataSize = 0;
  Status = gBS->StartImage (BootMgrImage, &ExitDataSize, &ExitData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WinBootMgr: StartImage failed (%r)\n", Status));
    if (ExitData != NULL) {
      gBS->FreePool (ExitData);
    }
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Entry point for the Windows Boot Manager.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
WinBootMgrEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  return BootWindows ();
}
