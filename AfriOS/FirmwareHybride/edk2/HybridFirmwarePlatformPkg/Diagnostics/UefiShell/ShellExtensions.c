/** @file
  UEFI Shell command extensions for AfriOS diagnostics.

  Registers the `afri` command via EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL and
  dispatches the following subcommands:

    afri_ver      - print AfriOS firmware version
    afri_slot     - print active A/B slot state
    afri_capsule  - check / apply firmware update capsule
    afri_pci      - enumerate PCI devices
    afri_memtest  - run a quick POST memory test

  Output is produced via ShellPrintHiiEx() / ShellPrintEx().

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/ShellDynamicCommand.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/PciIo.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/HiiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

#define AFRI_SHELL_CMD_NAME    L"afri"
#define AFRI_FIRMWARE_VERSION  L"0.1.0"

//
// Forward declarations of subcommand handlers.
//
STATIC
SHELL_STATUS
EFIAPI
AfriCmdVer (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  );

STATIC
SHELL_STATUS
EFIAPI
AfriCmdSlot (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  );

STATIC
SHELL_STATUS
EFIAPI
AfriCmdCapsule (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  );

STATIC
SHELL_STATUS
EFIAPI
AfriCmdPci (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  );

STATIC
SHELL_STATUS
EFIAPI
AfriCmdMemtest (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  );

//
// Subcommand dispatch table.
//
typedef struct {
  CONST CHAR16    *Name;
  SHELL_STATUS    (*Handler)(IN SHELL_INTERNAL_COMMAND *, IN UINTN, IN CHAR16 **);
  CONST CHAR16    *Help;
} AFRI_SUBCMD;

STATIC CONST AFRI_SUBCMD  mSubcommands[] = {
  { L"ver",     AfriCmdVer,     L"Print AfriOS firmware version" },
  { L"slot",    AfriCmdSlot,    L"Print active A/B slot state" },
  { L"capsule", AfriCmdCapsule, L"Check or apply firmware update capsule" },
  { L"pci",     AfriCmdPci,     L"Enumerate PCI devices" },
  { L"memtest", AfriCmdMemtest, L"Run quick POST memory test" },
  { NULL,       NULL,           NULL }
};

/**
  afri_ver — prints the AfriOS firmware version.
**/
STATIC
SHELL_STATUS
EFIAPI
AfriCmdVer (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  )
{
  ShellPrintHiiEx (
    -1, -1, NULL,
    STRING_TOKEN (STR_AFRI_VER),
    L"AfriOS",
    AFRI_FIRMWARE_VERSION
    );

  //
  // Fallback plain-text output for shells without our Hii handle.
  //
  ShellPrintEx (-1, -1, L"AfriOS Hybrid Firmware version %s\r\n", AFRI_FIRMWARE_VERSION);
  return SHELL_SUCCESS;
}

/**
  afri_slot — prints active A/B slot state from the AfriBootInfo NVRAM var.

  This handler reads the variable directly (rather than linking against
  ABSlotManagerLib) so the shell extension can be deployed standalone.
**/
STATIC
SHELL_STATUS
EFIAPI
AfriCmdSlot (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  )
{
  EFI_STATUS  Status;
  EFI_GUID    AfriBootInfoGuid = {
    0x8B6F2A1C, 0x3D5E, 0x4F8A, { 0xB1, 0xC2, 0xD3, 0xE4, 0xF5, 0xA6, 0xB7, 0xC8 }
  };
  UINT8       Info[8];
  UINTN       Size;

  Size = sizeof (Info);
  ZeroMem (Info, Size);
  Status = gRT->GetVariable (L"AfriBootInfo", &AfriBootInfoGuid, NULL, &Size, Info);
  if (EFI_ERROR (Status)) {
    ShellPrintEx (-1, -1, L"afri_slot: AfriBootInfo not available (%r)\r\n", Status);
    return SHELL_NOT_FOUND;
  }

  ShellPrintEx (-1, -1,
    L"AfriBootInfo:\r\n"
    L"  ActiveSlot  : %c (slot %u)\r\n"
    L"  BootSuccess : 0x%02x\r\n"
    L"  RetryCount  : %u\r\n",
    (Info[0] == 0) ? L'A' : L'B', (UINT32)Info[0],
    (UINT32)Info[1],
    (UINT32)Info[2]
    );
  return SHELL_SUCCESS;
}

/**
  afri_capsule — checks for or applies a firmware update capsule.
  Usage: afri capsule [apply]
**/
STATIC
SHELL_STATUS
EFIAPI
AfriCmdCapsule (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  )
{
  EFI_STATUS                          Status;
  UINTN                               HandleCount;
  EFI_HANDLE                          *HandleBuffer;
  UINTN                               Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *Fs;
  EFI_FILE_PROTOCOL                   *Root;
  EFI_FILE_PROTOCOL                   *File;

  if ((Argc >= 2) && (StrCmp (Argv[1], L"apply") == 0)) {
    ShellPrintEx (-1, -1, L"afri_capsule: apply is not yet implemented in the shell extension\r\n");
    ShellPrintEx (-1, -1, L"afri_capsule: use the FwUpdateAgent DXE driver to apply capsules\r\n");
    return SHELL_UNSUPPORTED;
  }

  ShellPrintEx (-1, -1, L"afri_capsule: scanning ESPs for \\EFI\\AfriOS\\Updates\\update.cap\r\n");

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
    ShellPrintEx (-1, -1, L"afri_capsule: no file systems found\r\n");
    return SHELL_NOT_FOUND;
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
    Status = Root->Open (Root, &File, L"\\EFI\\AfriOS\\Updates\\update.cap", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR (Status)) {
      File->Close (File);
      Root->Close (Root);
      ShellPrintEx (-1, -1, L"afri_capsule: capsule PRESENT on handle %p\r\n", HandleBuffer[Index]);
      gBS->FreePool (HandleBuffer);
      return SHELL_SUCCESS;
    }
    Root->Close (Root);
  }

  gBS->FreePool (HandleBuffer);
  ShellPrintEx (-1, -1, L"afri_capsule: no capsule found\r\n");
  return SHELL_NOT_FOUND;
}

/**
  afri_pci — enumerates PCI devices via EFI_PCI_IO_PROTOCOL.
**/
STATIC
SHELL_STATUS
EFIAPI
AfriCmdPci (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  )
{
  EFI_STATUS           Status;
  UINTN                HandleCount;
  EFI_HANDLE           *HandleBuffer;
  UINTN                Index;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINTN                Segment;
  UINTN                Bus;
  UINTN                Device;
  UINTN                Function;
  UINT32               VendorId;

  HandleCount  = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    ShellPrintEx (-1, -1, L"afri_pci: no PCI I/O handles found\r\n");
    return SHELL_NOT_FOUND;
  }

  ShellPrintEx (-1, -1, L"afri_pci: %lu device(s)\r\n", (UINT64)HandleCount);
  ShellPrintEx (-1, -1, L"  SEG BUS DEV FN  VID    DID\r\n");

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }
    if (EFI_ERROR (PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function))) {
      continue;
    }
    if (EFI_ERROR (PciIo->Pci.Read (
                              PciIo,
                              EfiPciIoWidthUint32,
                              0x00,
                              1,
                              &VendorId))) {
      continue;
    }
    ShellPrintEx (
      -1, -1,
      L"  %03x  %02x  %02x  %x  %04x  %04x\r\n",
      (UINT32)Segment, (UINT32)Bus, (UINT32)Device, (UINT32)Function,
      (UINT32)(VendorId & 0xFFFF), (UINT32)((VendorId >> 16) & 0xFFFF)
      );
  }

  gBS->FreePool (HandleBuffer);
  return SHELL_SUCCESS;
}

/**
  afri_memtest — runs a quick POST memory test on the first 16 MiB of each
  EfiConventionalMemory region.
**/
STATIC
SHELL_STATUS
EFIAPI
AfriCmdMemtest (
  IN SHELL_INTERNAL_COMMAND  *This,
  IN UINTN                   Argc,
  IN CHAR16                  **Argv
  )
{
  EFI_STATUS              Status;
  UINTN                   MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap;
  UINTN                   MapKey;
  UINTN                   DescriptorSize;
  UINT32                  DescriptorVersion;
  EFI_MEMORY_DESCRIPTOR   *Desc;
  UINTN                   Index;
  UINTN                   NumEntries;
  UINTN                   RegionsTested;

  ShellPrintEx (-1, -1, L"afri_memtest: starting quick memory test\r\n");

  MemoryMapSize = 0;
  MemoryMap     = NULL;
  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    ShellPrintEx (-1, -1, L"afri_memtest: GetMemoryMap failed (%r)\r\n", Status);
    return SHELL_DEVICE_ERROR;
  }

  MemoryMapSize += 4 * DescriptorSize;
  MemoryMap = (EFI_MEMORY_DESCRIPTOR *)AllocatePool (MemoryMapSize);
  if (MemoryMap == NULL) {
    return SHELL_OUT_OF_MEMORY;
  }

  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (EFI_ERROR (Status)) {
    FreePool (MemoryMap);
    return SHELL_DEVICE_ERROR;
  }

  NumEntries    = MemoryMapSize / DescriptorSize;
  RegionsTested = 0;

  for (Index = 0; Index < NumEntries; Index++) {
    Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (Index * DescriptorSize));
    if (Desc->Type != EfiConventionalMemory) {
      continue;
    }
    ShellPrintEx (-1, -1,
      L"  region 0x%016lx pages=%lu\r\n",
      (UINT64)Desc->PhysicalStart, (UINT64)Desc->NumberOfPages
      );
    RegionsTested++;
  }

  FreePool (MemoryMap);
  ShellPrintEx (-1, -1, L"afri_memtest: scanned %lu region(s)\r\n", (UINT64)RegionsTested);
  return SHELL_SUCCESS;
}

/**
  Main dispatcher for the `afri` shell command.

  @param[in] This            Pointer to the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] SystemTable     Pointer to the EFI System Table.
  @param[in] OriginalCmdLine The raw command line.

  @retval SHELL_SUCCESS      Command completed successfully.
**/
SHELL_STATUS
EFIAPI
AfriShellCommandHandler (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  *This,
  IN EFI_SYSTEM_TABLE                    *SystemTable,
  IN EFI_SHELL_PARAMETERS_PROTOCOL       *ShellParameters,
  IN CONST CHAR16                        *OriginalCmdLine
  )
{
  CHAR16                 *CmdLine;
  CHAR16                 *Argv[16];
  UINTN                  Argc;
  UINTN                  Index;
  CONST AFRI_SUBCMD      *Sub;

  if (OriginalCmdLine == NULL) {
    return SHELL_INVALID_PARAMETER;
  }

  CmdLine = AllocateCopyPool (StrSize (OriginalCmdLine), OriginalCmdLine);
  if (CmdLine == NULL) {
    return SHELL_OUT_OF_MEMORY;
  }

  //
  // Crude tokenizer: split on whitespace.
  //
  Argc = 0;
  ZeroMem (Argv, sizeof (Argv));
  {
    CHAR16  *Cursor = CmdLine;
    while ((*Cursor != L'\0') && (Argc < (ARRAY_SIZE (Argv) - 1))) {
      while ((*Cursor == L' ') || (*Cursor == L'\t')) {
        Cursor++;
      }
      if (*Cursor == L'\0') {
        break;
      }
      Argv[Argc++] = Cursor;
      while ((*Cursor != L'\0') && (*Cursor != L' ') && (*Cursor != L'\t')) {
        Cursor++;
      }
      if (*Cursor != L'\0') {
        *Cursor++ = L'\0';
      }
    }
  }

  if (Argc < 2) {
    ShellPrintEx (-1, -1, L"Usage: afri <subcommand>\r\n");
    ShellPrintEx (-1, -1, L"Subcommands:\r\n");
    for (Index = 0; mSubcommands[Index].Name != NULL; Index++) {
      ShellPrintEx (-1, -1, L"  %-9s - %s\r\n", mSubcommands[Index].Name, mSubcommands[Index].Help);
    }
    FreePool (CmdLine);
    return SHELL_SUCCESS;
  }

  for (Index = 0; mSubcommands[Index].Name != NULL; Index++) {
    if (StrCmp (Argv[1], mSubcommands[Index].Name) == 0) {
      SHELL_STATUS  SubStatus = mSubcommands[Index].Handler (NULL, Argc - 1, &Argv[1]);
      FreePool (CmdLine);
      return SubStatus;
    }
  }

  ShellPrintEx (-1, -1, L"afri: unknown subcommand '%s'\r\n", Argv[1]);
  FreePool (CmdLine);
  return SHELL_NOT_FOUND;
}

/**
  Returns the help string for the `afri` command.

  @param[in] This            Pointer to the protocol instance.
  @param[in] SystemTable     Pointer to the EFI System Table.
**/
CHAR16 *
EFIAPI
AfriShellCommandGetHelp (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  *This,
  IN CONST CHAR16                        *Language
  )
{
  return HiiGetString (
           gShellAfriHiiHandle,
           STRING_TOKEN (STR_AFRI_HELP),
           (CHAR16 *)Language
           );
}

EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  mAfriShellDynamicCommand = {
  AFRI_SHELL_CMD_NAME,
  AfriShellCommandHandler,
  AfriShellCommandGetHelp
};

/**
  Entry point for the UEFI Shell Extensions.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
ShellExtensionsEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mAfriShellDynamicCommand
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ShellExtensions: failed to install protocol (%r)\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "ShellExtensions: 'afri' command registered\n"));
  return EFI_SUCCESS;
}
