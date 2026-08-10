/** @file
  Boot Script Execution Engine for Hybrid Firmware.

  Implements a tiny .nsh-like scripting engine that supports a minimal subset
  of commands sufficient to drive the platform at boot:

    boot <option>          — boot the named boot option
    delay <seconds>        — sleep for N seconds
    echo <text>            — print text to console
    setvar <name> <value>  — set an NVRAM environment variable
    reboot [warm|cold]     — reset the system

  The engine is line-oriented; comments start with '#'.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>

#define BOOT_SCRIPT_MAX_LINE     512
#define BOOT_SCRIPT_MAX_TOKENS   8
#define BOOT_SCRIPT_MAX_ARG_LEN  128

//
// Tokenized representation of one script line.
//
typedef struct {
  CHAR8   Tokens[BOOT_SCRIPT_MAX_TOKENS][BOOT_SCRIPT_MAX_ARG_LEN];
  UINTN   TokenCount;
} SCRIPT_LINE;

/**
  Tokenizes a single ASCII line into space-separated tokens.

  @param[in]  Line   The input line (will be modified in-place).
  @param[out] Parsed Receives the tokenized line.

  @retval EFI_SUCCESS   Tokenization succeeded.
**/
STATIC
EFI_STATUS
TokenizeLine (
  IN OUT CHAR8        *Line,
  OUT    SCRIPT_LINE  *Parsed
  )
{
  CHAR8   *Cursor;
  UINTN   TokenLen;

  ZeroMem (Parsed, sizeof (*Parsed));
  Cursor = Line;

  while ((*Cursor != '\0') && (Parsed->TokenCount < BOOT_SCRIPT_MAX_TOKENS)) {
    while ((*Cursor == ' ') || (*Cursor == '\t')) {
      Cursor++;
    }
    if (*Cursor == '\0') {
      break;
    }
    TokenLen = 0;
    while ((*Cursor != '\0') && (*Cursor != ' ') && (*Cursor != '\t')
           && (TokenLen < (BOOT_SCRIPT_MAX_ARG_LEN - 1))) {
      Parsed->Tokens[Parsed->TokenCount][TokenLen++] = *Cursor++;
    }
    Parsed->Tokens[Parsed->TokenCount][TokenLen] = '\0';
    Parsed->TokenCount++;
  }

  return EFI_SUCCESS;
}

/**
  Implements the `boot <option>` command: looks up the named boot option and
  invokes EfiBootManagerBoot() on it.

  @param[in] Parsed   The tokenized line.

  @retval EFI_SUCCESS Boot option started.
**/
STATIC
EFI_STATUS
ScriptCmdBoot (
  IN SCRIPT_LINE  *Parsed
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *Options;
  UINTN                         OptionCount;
  UINTN                         Index;
  EFI_STATUS                    Status;
  CHAR16                        OptionName[BOOT_SCRIPT_MAX_ARG_LEN];

  if (Parsed->TokenCount < 2) {
    gST->ConOut->OutputString (gST->ConOut, L"boot: missing option name\r\n");
    return EFI_INVALID_PARAMETER;
  }

  //
  // Convert the ASCII option name to UTF-16 for the BootManager library.
  //
  AsciiStrToUnicodeStrS (Parsed->Tokens[1], OptionName, BOOT_SCRIPT_MAX_ARG_LEN);

  Options = EfiBootManagerGetLoadOptions (&OptionCount, LoadOptionTypeBoot);
  if ((Options == NULL) || (OptionCount == 0)) {
    gST->ConOut->OutputString (gST->ConOut, L"boot: no boot options available\r\n");
    return EFI_NOT_FOUND;
  }

  for (Index = 0; Index < OptionCount; Index++) {
    if (StrCmp (Options[Index].Description, OptionName) == 0) {
      DEBUG ((DEBUG_INFO, "BootScripts: booting %s\n", OptionName));
      Status = EfiBootManagerBoot (&Options[Index]);
      EfiBootManagerFreeLoadOptions (Options, OptionCount);
      return Status;
    }
  }

  EfiBootManagerFreeLoadOptions (Options, OptionCount);
  gST->ConOut->OutputString (gST->ConOut, L"boot: option not found\r\n");
  return EFI_NOT_FOUND;
}

/**
  Implements the `delay <seconds>` command: sleeps for N seconds.

  @param[in] Parsed   The tokenized line.

  @retval EFI_SUCCESS Sleep completed.
**/
STATIC
EFI_STATUS
ScriptCmdDelay (
  IN SCRIPT_LINE  *Parsed
  )
{
  UINTN       Seconds;
  EFI_STATUS  Status;
  CHAR16      Msg[64];

  if (Parsed->TokenCount < 2) {
    return EFI_INVALID_PARAMETER;
  }

  Seconds = AsciiStrDecimalToUintn (Parsed->Tokens[1]);
  if (Seconds > 60) {
    Seconds = 60;
  }

  UnicodeSPrint (Msg, sizeof (Msg), L"delay: sleeping %lu seconds...\r\n", (UINT64)Seconds);
  gST->ConOut->OutputString (gST->ConOut, Msg);

  Status = gBS->Stall ((UINT32)Seconds * 1000000);
  return Status;
}

/**
  Implements the `echo <text>` command: prints the rest of the line.

  @param[in] Parsed   The tokenized line.

  @retval EFI_SUCCESS Echo completed.
**/
STATIC
EFI_STATUS
ScriptCmdEcho (
  IN SCRIPT_LINE  *Parsed
  )
{
  UINTN   Index;
  CHAR16  Line[BOOT_SCRIPT_MAX_LINE];

  Line[0] = L'\0';
  for (Index = 1; Index < Parsed->TokenCount; Index++) {
    CHAR16  TokenW[BOOT_SCRIPT_MAX_ARG_LEN];
    AsciiStrToUnicodeStrS (Parsed->Tokens[Index], TokenW, BOOT_SCRIPT_MAX_ARG_LEN);
    StrCatS (Line, BOOT_SCRIPT_MAX_LINE, TokenW);
    if (Index < (Parsed->TokenCount - 1)) {
      StrCatS (Line, BOOT_SCRIPT_MAX_LINE, L" ");
    }
  }
  StrCatS (Line, BOOT_SCRIPT_MAX_LINE, L"\r\n");
  gST->ConOut->OutputString (gST->ConOut, Line);
  return EFI_SUCCESS;
}

/**
  Implements the `setvar <name> <value>` command: writes an NVRAM variable.

  @param[in] Parsed   The tokenized line.

  @retval EFI_SUCCESS Variable written.
**/
STATIC
EFI_STATUS
ScriptCmdSetvar (
  IN SCRIPT_LINE  *Parsed
  )
{
  EFI_STATUS    Status;
  CHAR16        NameW[BOOT_SCRIPT_MAX_ARG_LEN];
  CHAR8         *Value;
  UINTN         ValueLen;
  EFI_GUID      VendorGuid;

  if (Parsed->TokenCount < 3) {
    return EFI_INVALID_PARAMETER;
  }

  AsciiStrToUnicodeStrS (Parsed->Tokens[1], NameW, BOOT_SCRIPT_MAX_ARG_LEN);
  Value    = Parsed->Tokens[2];
  ValueLen = AsciiStrLen (Value);

  //
  // Store under the AfriOS vendor GUID (uses gEfiGlobalVariableGuid here so
  // that the var is visible to the standard boot manager tooling too).
  //
  CopyMem (&VendorGuid, &gEfiGlobalVariableGuid, sizeof (VendorGuid));

  Status = gRT->SetVariable (
                  NameW,
                  &VendorGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  ValueLen,
                  Value
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BootScripts: setvar failed (%r)\n", Status));
  }
  return Status;
}

/**
  Implements the `reboot [warm|cold]` command: resets the system.

  @param[in] Parsed   The tokenized line.

  @retval EFI_NOT_RETURN   System reset did not return.
**/
STATIC
EFI_STATUS
ScriptCmdReboot (
  IN SCRIPT_LINE  *Parsed
  )
{
  EFI_RESET_TYPE  ResetType;

  ResetType = EfiResetCold;
  if ((Parsed->TokenCount >= 2) && (AsciiStrCmp (Parsed->Tokens[1], "warm") == 0)) {
    ResetType = EfiResetWarm;
  }

  DEBUG ((DEBUG_INFO, "BootScripts: reboot (%d)\n", (UINT32)ResetType));
  gRT->ResetSystem (ResetType, EFI_SUCCESS, 0, NULL);

  return EFI_NOT_FOUND;
}

/**
  Executes a single parsed script line.

  @param[in] Parsed   The tokenized line.

  @retval EFI_SUCCESS         Command completed.
  @retval EFI_UNSUPPORTED     Unknown command.
  @retval EFI_NOT_FOUND       Reboot requested (did not return).
**/
STATIC
EFI_STATUS
ExecuteLine (
  IN SCRIPT_LINE  *Parsed
  )
{
  if (Parsed->TokenCount == 0) {
    return EFI_SUCCESS;
  }

  if (AsciiStrCmp (Parsed->Tokens[0], "boot") == 0) {
    return ScriptCmdBoot (Parsed);
  }
  if (AsciiStrCmp (Parsed->Tokens[0], "delay") == 0) {
    return ScriptCmdDelay (Parsed);
  }
  if (AsciiStrCmp (Parsed->Tokens[0], "echo") == 0) {
    return ScriptCmdEcho (Parsed);
  }
  if (AsciiStrCmp (Parsed->Tokens[0], "setvar") == 0) {
    return ScriptCmdSetvar (Parsed);
  }
  if (AsciiStrCmp (Parsed->Tokens[0], "reboot") == 0) {
    return ScriptCmdReboot (Parsed);
  }

  DEBUG ((DEBUG_WARN, "BootScripts: unknown command '%a'\n", Parsed->Tokens[0]));
  return EFI_UNSUPPORTED;
}

/**
  Executes a boot script from a memory buffer.

  @param[in] Script   Null-terminated ASCII script text.

  @retval EFI_SUCCESS Script executed.
**/
EFI_STATUS
EFIAPI
BootScriptExecute (
  IN CONST CHAR8  *Script
  )
{
  CHAR8       *Buffer;
  CHAR8       *Line;
  CHAR8       *Cursor;
  SCRIPT_LINE Parsed;
  EFI_STATUS  Status;
  UINTN       LineNo;

  if (Script == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Take a mutable copy so we can split into lines without touching caller data.
  //
  Buffer = AllocateCopyPool (AsciiStrSize (Script), Script);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  LineNo  = 0;
  Cursor  = Buffer;
  while (*Cursor != '\0') {
    Line = Cursor;
    while ((*Cursor != '\0') && (*Cursor != '\n') && (*Cursor != '\r')) {
      Cursor++;
    }
    if (*Cursor != '\0') {
      *Cursor = '\0';
      Cursor++;
      if ((*Cursor == '\n') || (*Cursor == '\r')) {
        Cursor++;
      }
    }

    LineNo++;

    if ((Line[0] == '#') || (Line[0] == '\0')) {
      continue;
    }

    Status = TokenizeLine (Line, &Parsed);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = ExecuteLine (&Parsed);
    if (Status == EFI_NOT_FOUND) {
      //
      // Reboot was requested and did not return — unwind.
      //
      FreePool (Buffer);
      return EFI_SUCCESS;
    }
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "BootScripts: line %lu failed (%r)\n", (UINT64)LineNo, Status));
    }
  }

  FreePool (Buffer);
  return EFI_SUCCESS;
}

/**
  Entry point for the Boot Scripts module.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
BootScriptsEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "BootScripts: ready (script engine)\n"));
  return EFI_SUCCESS;
}
