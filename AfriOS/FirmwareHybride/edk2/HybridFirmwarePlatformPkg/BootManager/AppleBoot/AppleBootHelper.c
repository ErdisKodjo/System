/** @file
  Apple Boot Helper for Hybrid Firmware.

  This module uses the ConfigPlistParser to read OpenCore/Clover-style boot
  arguments, locates the macOS boot loader (\System\Library\CoreServices\boot.efi)
  on the ESP, loads it via gBS->LoadImage, applies a stubbed kext-patch hook
  table, then starts the image.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>

//
// Path to the macOS boot loader on the ESP. macOS uses HFS+/APFS volumes; the
// ESP-relative path below matches the layout used by OpenCore when a FAT32
// shim is used to chain-load boot.efi.
//
#define APPLE_BOOT_EFI_PATH    L"\\System\\Library\\CoreServices\\boot.efi"
#define APPLE_CONFIG_PATH      L"\\EFI\\OC\\config.plist"
#define APPLE_MAX_BOOT_ARGS    256

//
// Forward declaration from ConfigPlistParser.c — we treat it as opaque here.
//
typedef struct {
  VOID    *Root;
} CONFIG_PLIST;

extern
EFI_STATUS
EFIAPI
ParseConfigPlist (
  IN  CONST VOID    *Buffer,
  IN  UINTN         Length,
  OUT CONFIG_PLIST  *Plist
  );

extern
EFI_STATUS
EFIAPI
ConfigGetString (
  IN  CONFIG_PLIST  *Plist,
  IN  CONST CHAR8   *Path,
  OUT CHAR8         *Value
  );

extern
EFI_STATUS
EFIAPI
ConfigGetInteger (
  IN  CONFIG_PLIST  *Plist,
  IN  CONST CHAR8   *Path,
  OUT UINT64        *Value
  );

//
// Stubbed kext-patch hook table. A real implementation would walk the loaded
// boot.efi image memory and patch symbol references; here we only record the
// patch descriptor for debugging.
//
typedef struct {
  CONST CHAR8    *KextName;
  CONST CHAR8    *SymbolName;
  UINT32         PatchOffset;
  UINT8          PatchBytes[8];
  UINTN          PatchLen;
} KEXT_PATCH;

STATIC KEXT_PATCH  mKextPatchTable[] = {
  { "Lilu",      "_lilu_init",       0x0000, { 0x90, 0x90, 0x90, 0x90 }, 4 },
  { "WhateverGreen", "_wg_init",     0x0000, { 0x90, 0x90 },             2 },
  { "VirtualSMC", "_vsmc_init",      0x0000, { 0xC3 },                   1 },
  { NULL, NULL, 0, { 0 }, 0 }
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
ReadEntireFile (
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
  Loads config.plist from the ESP and parses it.

  @param[in]  FsHandle   The ESP file system handle.
  @param[out] Plist      Receives the parsed plist tree.

  @retval EFI_SUCCESS    Plist parsed.
**/
STATIC
EFI_STATUS
LoadAppleConfig (
  IN  EFI_HANDLE    FsHandle,
  OUT CONFIG_PLIST  *Plist
  )
{
  EFI_STATUS                          Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *Fs;
  VOID                                *Buffer;
  UINTN                               Size;

  Status = gBS->HandleProtocol (FsHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Buffer = NULL;
  Size   = 0;
  Status = ReadEntireFile (Fs, APPLE_CONFIG_PATH, &Buffer, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "AppleBootHelper: config.plist not found (%r)\n", Status));
    return Status;
  }

  Status = ParseConfigPlist (Buffer, Size, Plist);
  FreePool (Buffer);
  return Status;
}

/**
  Applies the kext-patch hook table to the loaded boot.efi image.

  This is a stub: in a full implementation, we would walk the Mach-O load
  commands of boot.efi (or kernel) to resolve each kext's __TEXT segment and
  patch the recorded offsets. Here we only log the patch list for debugging.

  @param[in]  ImageBase   Base of the loaded image (from LoadedImage).
  @param[in]  ImageSize   Size of the loaded image.

  @retval EFI_SUCCESS     Patches applied (or stubbed).
**/
STATIC
EFI_STATUS
ApplyKextPatches (
  IN VOID    *ImageBase,
  IN UINTN   ImageSize
  )
{
  UINTN       Index;
  KEXT_PATCH  *Patch;

  DEBUG ((DEBUG_INFO, "AppleBootHelper: Applying kext patches (image %p, size 0x%lx)\n",
          ImageBase, (UINT64)ImageSize));

  for (Index = 0; mKextPatchTable[Index].KextName != NULL; Index++) {
    Patch = &mKextPatchTable[Index];
    DEBUG ((DEBUG_INFO, "  [%a] %a @+0x%08x len=%lu\n",
            Patch->KextName, Patch->SymbolName, Patch->PatchOffset, (UINT64)Patch->PatchLen));
    //
    // A real implementation would do:
    //   CopyMem ((UINT8 *)ImageBase + Patch->PatchOffset, Patch->PatchBytes, Patch->PatchLen);
    // after validating the offset is within ImageSize and within the right kext segment.
    //
    if (Patch->PatchOffset >= ImageSize) {
      DEBUG ((DEBUG_WARN, "  patch offset out of range, skipping\n"));
      continue;
    }
  }

  return EFI_SUCCESS;
}

/**
  Reads the boot args from config.plist (Kernel.BootArgs / boot-args).

  @param[in]  Plist       Parsed config.plist.
  @param[out] BootArgs    Buffer receiving the boot args.

  @retval EFI_SUCCESS     Boot args retrieved (or empty if not present).
**/
STATIC
EFI_STATUS
ReadBootArgs (
  IN  CONFIG_PLIST  *Plist,
  OUT CHAR8         *BootArgs
  )
{
  EFI_STATUS  Status;

  BootArgs[0] = '\0';

  Status = ConfigGetString (Plist, "Kernel.BootArgs", BootArgs);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AppleBootHelper: boot-args (Kernel.BootArgs) = \"%a\"\n", BootArgs));
    return EFI_SUCCESS;
  }

  Status = ConfigGetString (Plist, "boot-args", BootArgs);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "AppleBootHelper: boot-args (boot-args) = \"%a\"\n", BootArgs));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "AppleBootHelper: no boot-args in config.plist\n"));
  return EFI_NOT_FOUND;
}

/**
  Locates the ESP handle that contains the macOS boot.efi loader.

  @param[out] FsHandle   Pointer to the ESP handle.

  @retval EFI_SUCCESS    Handle found.
**/
STATIC
EFI_STATUS
LocateAppleEsp (
  OUT EFI_HANDLE  *FsHandle
  )
{
  EFI_STATUS                          Status;
  UINTN                               HandleCount;
  EFI_HANDLE                          *HandleBuffer;
  UINTN                               Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *Fs;
  EFI_FILE_PROTOCOL                   *Root;
  EFI_FILE_PROTOCOL                   *BootEfi;

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
    Status = Root->Open (Root, &BootEfi, APPLE_BOOT_EFI_PATH, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR (Status)) {
      BootEfi->Close (BootEfi);
      Root->Close (Root);
      *FsHandle = HandleBuffer[Index];
      gBS->FreePool (HandleBuffer);
      return EFI_SUCCESS;
    }
    Root->Close (Root);
  }

  gBS->FreePool (HandleBuffer);
  return EFI_NOT_FOUND;
}

/**
  Locates, loads, patches and starts the macOS boot.efi.

  @return EFI_SUCCESS if boot.efi was started.
**/
EFI_STATUS
EFIAPI
BootApple (
  VOID
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                FsHandle;
  EFI_HANDLE                BootEfiImage;
  EFI_DEVICE_PATH_PROTOCOL  *BootEfiDp;
  CONFIG_PLIST              Plist;
  CHAR8                     BootArgs[APPLE_MAX_BOOT_ARGS];
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

  DEBUG ((DEBUG_INFO, "AppleBootHelper: Entry\n"));

  FsHandle = NULL;
  Status = LocateAppleEsp (&FsHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AppleBootHelper: boot.efi not found on any ESP\n"));
    return Status;
  }

  ZeroMem (&Plist, sizeof (Plist));
  Status = LoadAppleConfig (FsHandle, &Plist);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "AppleBootHelper: continuing without config.plist\n"));
  } else {
    ReadBootArgs (&Plist, BootArgs);
  }

  BootEfiDp = FileDevicePath (FsHandle, APPLE_BOOT_EFI_PATH);
  if (BootEfiDp == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BootEfiImage = NULL;
  Status = gBS->LoadImage (FALSE, gImageHandle, BootEfiDp, NULL, 0, &BootEfiImage);
  gBS->FreePool (BootEfiDp);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AppleBootHelper: LoadImage failed (%r)\n", Status));
    return Status;
  }

  //
  // Apply kext patches to the loaded image before starting it.
  //
  Status = gBS->HandleProtocol (
                  BootEfiImage,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (!EFI_ERROR (Status)) {
    ApplyKextPatches (LoadedImage->ImageBase, LoadedImage->ImageSize);
  }

  DEBUG ((DEBUG_INFO, "AppleBootHelper: Starting boot.efi (image=%p)\n", BootEfiImage));
  Status = gBS->StartImage (BootEfiImage, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AppleBootHelper: StartImage failed (%r)\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Entry point for the Apple Boot Helper module.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
AppleBootHelperEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  return BootApple ();
}
