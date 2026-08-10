/** @file
  Simple text-mode Boot Menu UI for Hybrid Firmware.

  Lists boot options from EFI_BOOT_MANAGER_LOAD_OPTION, lets the user pick via
  arrow keys (read from EFI_SIMPLE_TEXT_INPUT_PROTOCOL), and invokes
  EfiBootManagerBoot() on the selected entry.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>

#define BOOT_MENU_TIMEOUT_SEC   5
#define BOOT_MENU_MAX_OPTIONS   32
#define BOOT_MENU_TITLE         L"AfriOS Boot Manager"

//
// Forward declarations.
//
STATIC EFI_STATUS  BootMenuDraw (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Options,
  IN UINTN                          Count,
  IN UINTN                          Selected
  );

STATIC EFI_STATUS  BootMenuWaitForInput (
  IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *TextIn,
  OUT EFI_INPUT_KEY                   *Key,
  IN  UINTN                           TimeoutSec
  );

/**
  Draws the boot menu to the console output device.

  @param[in] Options   Array of boot options.
  @param[in] Count     Number of entries in Options.
  @param[in] Selected  Index of the currently selected entry.

  @retval EFI_SUCCESS  Menu drawn successfully.
**/
STATIC
EFI_STATUS
BootMenuDraw (
  IN EFI_BOOT_MANAGER_LOAD_OPTION  *Options,
  IN UINTN                         Count,
  IN UINTN                         Selected
  )
{
  UINTN   Index;
  UINTN   Row;
  UINTN   Col;
  UINTN   Rows;
  UINTN   Cols;
  CHAR16  Line[128];

  gST->ConOut->Reset (gST->ConOut, FALSE);
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Cols, &Rows);

  gST->ConOut->SetAttribute (gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
  for (Row = 0; Row < Rows; Row++) {
    for (Col = 0; Col < Cols; Col++) {
      gST->ConOut->OutputString (gST->ConOut, L" ");
    }
  }

  gST->ConOut->SetCursorPosition (gST->ConOut, 2, 1);
  gST->ConOut->OutputString (gST->ConOut, BOOT_MENU_TITLE);
  gST->ConOut->SetCursorPosition (gST->ConOut, 2, 2);
  gST->ConOut->OutputString (gST->ConOut, L"=====================================");

  for (Index = 0; Index < Count && Index < BOOT_MENU_MAX_OPTIONS; Index++) {
    if (Index == Selected) {
      gST->ConOut->SetAttribute (gST->ConOut, EFI_BLACK | EFI_BACKGROUND_WHITE);
    } else {
      gST->ConOut->SetAttribute (gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
    }

    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"  [%2lu] %s",
      (UINT64)Index,
      (Options[Index].Description != NULL) ? Options[Index].Description : L"(no description)"
      );
    gST->ConOut->SetCursorPosition (gST->ConOut, 2, (UINT32)(4 + Index));
    gST->ConOut->OutputString (gST->ConOut, Line);
  }

  gST->ConOut->SetAttribute (gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
  gST->ConOut->SetCursorPosition (gST->ConOut, 2, (UINT32)(4 + Count + 1));
  gST->ConOut->OutputString (gST->ConOut, L"Up/Down to navigate, Enter to boot, Esc to abort");

  return EFI_SUCCESS;
}

/**
  Waits for a single key press, with an optional timeout in seconds.

  @param[in]  TextIn       Pointer to the Simple Text Input protocol.
  @param[out] Key          Receives the pressed key.
  @param[in]  TimeoutSec   Timeout in seconds (0 = wait forever).

  @retval EFI_SUCCESS      A key was pressed.
**/
STATIC
EFI_STATUS
BootMenuWaitForInput (
  IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *TextIn,
  OUT EFI_INPUT_KEY                   *Key,
  IN  UINTN                           TimeoutSec
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   TimerEvent;
  EFI_EVENT   WaitList[2];
  UINTN       Index;
  UINTN       Elapsed;

  ZeroMem (Key, sizeof (*Key));

  if (TimeoutSec == 0) {
    Status = gST->ConIn->WaitForEvent (1, &TextIn->WaitForKey, &Index);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    return TextIn->ReadKeyStroke (TextIn, Key);
  }

  Status = gBS->CreateEvent (EVT_TIMER, 0, NULL, NULL, &TimerEvent);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->SetTimer (TimerEvent, TimerPeriodic, 10000000); // 1 s
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (TimerEvent);
    return Status;
  }

  WaitList[0] = TextIn->WaitForKey;
  WaitList[1] = TimerEvent;
  Elapsed     = 0;

  while (Elapsed < TimeoutSec) {
    Status = gST->ConIn->WaitForEvent (2, WaitList, &Index);
    if (EFI_ERROR (Status)) {
      break;
    }
    if (Index == 0) {
      Status = TextIn->ReadKeyStroke (TextIn, Key);
      gBS->CloseEvent (TimerEvent);
      return Status;
    }
    Elapsed++;
  }

  gBS->CloseEvent (TimerEvent);
  return EFI_TIMEOUT;
}

/**
  Main loop for the boot menu: lists options, reads arrow/enter keys, and
  invokes EfiBootManagerBoot() on the selected option.

  @retval EFI_SUCCESS  A boot option was selected and started.
**/
EFI_STATUS
EFIAPI
BootMenuShow (
  VOID
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *Options;
  UINTN                         OptionCount;
  UINTN                         Selected;
  EFI_INPUT_KEY                 Key;
  EFI_STATUS                    Status;
  BOOLEAN                       Done;

  DEBUG ((DEBUG_INFO, "BootMenuUi: enumerating boot options\n"));

  Options = EfiBootManagerGetLoadOptions (&OptionCount, LoadOptionTypeBoot);
  if ((Options == NULL) || (OptionCount == 0)) {
    DEBUG ((DEBUG_ERROR, "BootMenuUi: no boot options available\n"));
    gST->ConOut->OutputString (gST->ConOut, L"No boot options available\r\n");
    return EFI_NOT_FOUND;
  }

  Selected = 0;
  Done     = FALSE;

  BootMenuDraw (Options, OptionCount, Selected);

  while (!Done) {
    Status = BootMenuWaitForInput (gST->ConIn, &Key, BOOT_MENU_TIMEOUT_SEC);
    if (Status == EFI_TIMEOUT) {
      DEBUG ((DEBUG_INFO, "BootMenuUi: timeout, booting default entry %lu\n", (UINT64)Selected));
      Done = TRUE;
      break;
    }
    if (EFI_ERROR (Status)) {
      continue;
    }

    switch (Key.ScanCode) {
      case SCAN_UP:
        if (Selected > 0) {
          Selected--;
        } else {
          Selected = OptionCount - 1;
        }
        break;
      case SCAN_DOWN:
        if (Selected < (OptionCount - 1)) {
          Selected++;
        } else {
          Selected = 0;
        }
        break;
      case SCAN_ESC:
        EfiBootManagerFreeLoadOptions (Options, OptionCount);
        return EFI_ABORTED;
      default:
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          Done = TRUE;
        }
        break;
    }

    BootMenuDraw (Options, OptionCount, Selected);
  }

  DEBUG ((DEBUG_INFO, "BootMenuUi: booting option %lu (%s)\n",
          (UINT64)Selected, Options[Selected].Description));

  Status = EfiBootManagerBoot (&Options[Selected]);
  EfiBootManagerFreeLoadOptions (Options, OptionCount);
  return Status;
}

/**
  Entry point for the Boot Menu UI.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
BootMenuUiEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  return BootMenuShow ();
}
