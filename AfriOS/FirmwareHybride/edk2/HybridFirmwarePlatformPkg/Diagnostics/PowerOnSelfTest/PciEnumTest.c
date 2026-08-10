/** @file
  PCI Enumeration Test for Hybrid Firmware POST.

  Uses EFI_PCI_IO_PROTOCOL to scan all PCI devices (bus/dev/fn), reporting
  vendor/device IDs, class codes, and BARs to the debug log.

  Exposes:
    - PostPciEnumTest()  : runs the scan via LocateHandleBuffer.
    - PciPrintConfig()   : pretty-prints a single PCI device's config space.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/DevicePath.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <IndustryStandard/Pci22.h>

#define PCI_ENUM_MAX_BARS      6
#define PCI_ENUM_MAX_DEVICES   256

/**
  Reads a 32-bit PCI config register via PciIo.

  @param[in]  PciIo    Pointer to the EFI_PCI_IO_PROTOCOL.
  @param[in]  Offset   Register offset within config space.
  @param[out] Value    Receives the 32-bit value.

  @retval EFI_SUCCESS   Read succeeded.
**/
STATIC
EFI_STATUS
PciRead32 (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  IN  UINT32               Offset,
  OUT UINT32               *Value
  )
{
  return PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, Value);
}

/**
  Pretty-prints a single PCI device's identifying information.

  @param[in] PciIo   Pointer to the EFI_PCI_IO_PROTOCOL for the device.

  @retval EFI_SUCCESS   Config space printed successfully.
**/
EFI_STATUS
EFIAPI
PciPrintConfig (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  EFI_STATUS  Status;
  UINTN       Segment;
  UINTN       Bus;
  UINTN       Device;
  UINTN       Function;
  UINT32      VendorId;
  UINT32      DeviceId;
  UINT32      ClassCode;
  UINT32      Bar;
  UINTN       BarIndex;
  CHAR16      Line[256];

  if (PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "PciEnumTest: GetLocation failed (%r)\n", Status));
    return Status;
  }

  Status = PciRead32 (PciIo, PCI_VENDOR_ID_OFFSET, &VendorId);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  if ((VendorId & 0xFFFF) == 0xFFFF) {
    //
    // No device at this location — skip silently.
    //
    return EFI_NOT_FOUND;
  }

  PciRead32 (PciIo, PCI_DEVICE_ID_OFFSET, &DeviceId);
  PciRead32 (PciIo, PCI_CLASSCODE_OFFSET,  &ClassCode);

  UnicodeSPrint (
    Line,
    sizeof (Line),
    L"Pci: %04x:%02x:%02x.%x VID=0x%04x DID=0x%04x Class=0x%06x (%a/%a/%a)\r\n",
    (UINT32)Segment, (UINT32)Bus, (UINT32)Device, (UINT32)Function,
    (UINT32)(VendorId & 0xFFFF), (UINT32)(DeviceId & 0xFFFF),
    (UINT32)(ClassCode >> 8),
    "Generic", "Device", "Unknown"
    );
  gST->ConOut->OutputString (gST->ConOut, Line);

  DEBUG ((DEBUG_INFO,
          "PciEnumTest: %04x:%02x:%02x.%x VID=0x%04x DID=0x%04x Class=0x%06x\n",
          (UINT32)Segment, (UINT32)Bus, (UINT32)Device, (UINT32)Function,
          (UINT32)(VendorId & 0xFFFF), (UINT32)(DeviceId & 0xFFFF),
          (UINT32)(ClassCode >> 8)));

  //
  // Walk BAR0..BAR5 and print any that look valid.
  //
  for (BarIndex = 0; BarIndex < PCI_ENUM_MAX_BARS; BarIndex++) {
    Status = PciRead32 (PciIo, PCI_BASE_ADDRESSREG_OFFSET + (BarIndex * 4), &Bar);
    if (EFI_ERROR (Status) || (Bar == 0) || (Bar == 0xFFFFFFFF)) {
      continue;
    }
    if ((Bar & 0x01) != 0) {
      DEBUG ((DEBUG_INFO, "  BAR%lu: IO   0x%04x\n", (UINT64)BarIndex, (UINT32)(Bar & 0xFFFFFFFC)));
    } else {
      DEBUG ((DEBUG_INFO, "  BAR%lu: MEM  0x%08x (type=%u)\n",
              (UINT64)BarIndex, (UINT32)(Bar & 0xFFFFFFF0), (UINT32)((Bar >> 1) & 0x3)));
    }
  }

  return EFI_SUCCESS;
}

/**
  Scans all PCI devices using the PCI I/O protocol. Iterates over every handle
  producing EFI_PCI_IO_PROTOCOL and calls PciPrintConfig() on each.

  @param[out] DeviceCount   Optional pointer to receive the device count.

  @retval EFI_SUCCESS   Scan completed (some devices may still have failed).
**/
EFI_STATUS
EFIAPI
PostPciEnumTest (
  OUT UINTN  *DeviceCount OPTIONAL
  )
{
  EFI_STATUS           Status;
  UINTN                HandleCount;
  EFI_HANDLE           *HandleBuffer;
  UINTN                Index;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINTN                LocalCount;

  DEBUG ((DEBUG_INFO, "PciEnumTest: enumerating PCI I/O handles\n"));

  HandleCount  = 0;
  HandleBuffer = NULL;
  LocalCount   = 0;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    DEBUG ((DEBUG_WARN, "PciEnumTest: no PCI I/O handles found (%r)\n", Status));
    if (DeviceCount != NULL) {
      *DeviceCount = 0;
    }
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "PciEnumTest: %lu PCI I/O handle(s) found\n", (UINT64)HandleCount));

  for (Index = 0; Index < HandleCount && LocalCount < PCI_ENUM_MAX_DEVICES; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = PciPrintConfig (PciIo);
    if (!EFI_ERROR (Status)) {
      LocalCount++;
    }
  }

  gBS->FreePool (HandleBuffer);

  DEBUG ((DEBUG_INFO, "PciEnumTest: scan complete, %lu device(s) reported\n", (UINT64)LocalCount));

  if (DeviceCount != NULL) {
    *DeviceCount = LocalCount;
  }

  return EFI_SUCCESS;
}

/**
  Entry point for the PCI Enumeration Test module.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
PostPciEnumTestEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "PciEnumTest: ready (POST PCI enumeration test)\n"));
  return EFI_SUCCESS;
}
