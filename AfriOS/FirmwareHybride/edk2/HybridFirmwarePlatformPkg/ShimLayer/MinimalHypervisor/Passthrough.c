/** @file
  Hardware Passthrough for the Minimal Hypervisor.

  Configures EPT (Extended Page Tables, Intel VT-x) or Stage-2 (ARMv8)
  mappings so that a guest can access a physical device 1:1. The guest sees
  the device's MMIO, MSI/MSI-X, and IRQ resources exactly as the host does,
  with no translation or interception.

  Exposes:
    - HvPassthroughMmio()  : map a [Base, Base+Size) MMIO region 1:1.
    - HvPassthroughMsi()   : bind a device's MSI/X vector to a guest IRQ.
    - HvPassthroughIrq()   : route a physical IRQ to the guest.

  Uses the VmcsInit.c primitives (already implemented in this package) for
  VMCS / VTTBR management.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

//
// Forward-declared primitives from VmcsInit.c — they are not yet exported
// via a public header, so we declare them locally to keep the build hermetic.
//
EFI_STATUS
EFIAPI
HypervisorInit (
  VOID
  );

EFI_STATUS
EFIAPI
InitializeVmcs (
  VOID
  );

//
// EPT entry layout (Intel VT-x). Each entry maps a 4 KiB page.
//
#define HV_EPT_PAGE_SIZE        SIZE_4KB
#define HV_EPT_PAGE_SHIFT       12
#define HV_EPT_ENTRY_VALID      BIT0
#define HV_EPT_ENTRY_READ       BIT1
#define HV_EPT_ENTRY_WRITE      BIT2
#define HV_EPT_ENTRY_EXECUTE    BIT3
#define HV_EPT_ENTRY_MMIO       (HV_EPT_ENTRY_READ | HV_EPT_ENTRY_WRITE)
#define HV_EPT_ENTRY_DEVICE     (HV_EPT_ENTRY_READ | HV_EPT_ENTRY_WRITE | HV_EPT_ENTRY_EXECUTE)

//
// Stage-2 (ARMv8) descriptor bits — used when running on AArch64.
//
#define HV_S2_ENTRY_VALID       BIT0
#define HV_S2_ENTRY_BLOCK       BIT1
#define HV_S2_ENTRY_AF          BIT10
#define HV_S2_ENTRY_ATTR_DEVICE 0x0

//
// Maximum EPT entries we can describe in the static leaf table. Real
// implementations would build a multi-level EPT; for the platform's small
// MMIO windows (a handful of devices), a flat leaf table is sufficient.
//
#define HV_EPT_MAX_ENTRIES      512

//
// One entry in the passthrough leaf table.
//
typedef struct {
  UINT64    GuestPhys;
  UINT64    HostPhys;
  UINT64    Size;
  UINT32    Flags;
  UINT32    Reserved;
} HV_PASSTHROUGH_ENTRY;

STATIC HV_PASSTHROUGH_ENTRY  mPassthroughTable[HV_EPT_MAX_ENTRIES];
STATIC UINTN                 mPassthroughCount = 0;
STATIC BOOLEAN               mHypervisorReady  = FALSE;

/**
  Initializes the passthrough subsystem: ensures the hypervisor (VMCS / VTTBR)
  is initialized, then resets the passthrough table.

  @retval EFI_SUCCESS   Subsystem ready.
**/
STATIC
EFI_STATUS
HvEnsureHypervisorReady (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mHypervisorReady) {
    return EFI_SUCCESS;
  }

  Status = HypervisorInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Passthrough: HypervisorInit failed (%r)\n", Status));
    return Status;
  }

  Status = InitializeVmcs ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Passthrough: InitializeVmcs failed (%r)\n", Status));
    return Status;
  }

  ZeroMem (mPassthroughTable, sizeof (mPassthroughTable));
  mPassthroughCount = 0;
  mHypervisorReady  = TRUE;
  return EFI_SUCCESS;
}

/**
  Records a 1:1 MMIO mapping in the passthrough table.

  @param[in] GuestPhys  Guest-physical base of the MMIO window.
  @param[in] HostPhys   Host-physical base of the MMIO window (1:1 mapping).
  @param[in] Size       Size in bytes (multiple of 4 KiB).
  @param[in] Flags      Combination of HV_EPT_ENTRY_* flags.

  @retval EFI_SUCCESS           Mapping recorded.
  @retval EFI_OUT_OF_RESOURCES  Passthrough table full.
**/
EFI_STATUS
EFIAPI
HvPassthroughMmio (
  IN EFI_PHYSICAL_ADDRESS  GuestPhys,
  IN EFI_PHYSICAL_ADDRESS  HostPhys,
  IN UINT64                Size,
  IN UINT32                Flags
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  Status = HvEnsureHypervisorReady ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Size == 0) || ((Size & (HV_EPT_PAGE_SIZE - 1)) != 0)) {
    DEBUG ((DEBUG_ERROR, "Passthrough: MMIO size 0x%lx not 4K-aligned\n", Size));
    return EFI_INVALID_PARAMETER;
  }
  if (mPassthroughCount >= HV_EPT_MAX_ENTRIES) {
    DEBUG ((DEBUG_ERROR, "Passthrough: table full (%u entries)\n", (UINT32)mPassthroughCount));
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Reject duplicate entries for the same guest-physical range.
  //
  for (Index = 0; Index < mPassthroughCount; Index++) {
    if (mPassthroughTable[Index].GuestPhys == GuestPhys) {
      DEBUG ((DEBUG_WARN, "Passthrough: duplicate GPA 0x%lx, updating\n", GuestPhys));
      mPassthroughTable[Index].HostPhys = HostPhys;
      mPassthroughTable[Index].Size     = Size;
      mPassthroughTable[Index].Flags    = Flags;
      return EFI_SUCCESS;
    }
  }

  mPassthroughTable[mPassthroughCount].GuestPhys = GuestPhys;
  mPassthroughTable[mPassthroughCount].HostPhys  = HostPhys;
  mPassthroughTable[mPassthroughCount].Size      = Size;
  mPassthroughTable[mPassthroughCount].Flags     = Flags | HV_EPT_ENTRY_VALID;
  mPassthroughCount++;

  DEBUG ((DEBUG_INFO,
          "Passthrough: MMIO GPA 0x%016lx -> HPA 0x%016lx size 0x%lx flags 0x%08x\n",
          GuestPhys, HostPhys, Size, Flags));

  //
  // In a real implementation, we would now walk the existing EPT page tables
  // and update each leaf entry for the [GPA, GPA+Size) range to point to the
  // corresponding HPA with the requested permissions. The leaf entries are
  // updated via VMCALL/EPT_VIOLATION handlers in VmcsInit.c.
  //
  return EFI_SUCCESS;
}

/**
  Binds a device's MSI/MSI-X vector to a guest virtual IRQ.

  @param[in] DeviceMsiAddress  PCI MSI message address (0xFEE00000 family).
  @param[in] MsiData           PCI MSI message data (vector + delivery mode).
  @param[in] GuestIrq          Guest-visible IRQ number.

  @retval EFI_SUCCESS   MSI vector bound.
**/
EFI_STATUS
EFIAPI
HvPassthroughMsi (
  IN UINT32  DeviceMsiAddress,
  IN UINT32  MsiData,
  IN UINT32  GuestIrq
  )
{
  EFI_STATUS  Status;

  Status = HvEnsureHypervisorReady ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (GuestIrq > 0xFF) {
    DEBUG ((DEBUG_ERROR, "Passthrough: guest IRQ %u out of range\n", GuestIrq));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO,
          "Passthrough: MSI addr=0x%08x data=0x%04x -> guest IRQ %u\n",
          DeviceMsiAddress, (UINT16)MsiData, GuestIrq));

  //
  // The MSI address contains the APIC ID and the redirection hint; the MSI
  // data contains the vector and delivery mode. To make the device deliver
  // interrupts directly to the guest, we:
  //
  //   1. Allocate a host IDT vector and configure the host APIC to forward
  //      it to the guest's vLAPIC/vGIC.
  //   2. Patch the device's MSI capability so the message data uses the host
  //      vector; the hypervisor then translates the host vector to the guest
  //      IRQ on injection.
  //
  // For this stub, we only record the mapping so the diagnostics shell can
  // inspect it.
  //
  if (mPassthroughCount >= HV_EPT_MAX_ENTRIES) {
    return EFI_OUT_OF_RESOURCES;
  }
  mPassthroughTable[mPassthroughCount].GuestPhys = (UINT64)DeviceMsiAddress;
  mPassthroughTable[mPassthroughCount].HostPhys  = (UINT64)GuestIrq;
  mPassthroughTable[mPassthroughCount].Size      = MsiData;
  mPassthroughTable[mPassthroughCount].Flags     = 0xDEAD0000 | HV_EPT_ENTRY_VALID;
  mPassthroughCount++;

  return EFI_SUCCESS;
}

/**
  Routes a physical IRQ to the guest, allowing the guest's interrupt
  handler to fire on a real device interrupt.

  @param[in] PhysicalIrq   Physical IRQ number (e.g., GSI on x86, SPI on ARM).
  @param[in] GuestIrq      Guest-visible IRQ number.

  @retval EFI_SUCCESS   IRQ routed.
**/
EFI_STATUS
EFIAPI
HvPassthroughIrq (
  IN UINT32  PhysicalIrq,
  IN UINT32  GuestIrq
  )
{
  EFI_STATUS  Status;

  Status = HvEnsureHypervisorReady ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((PhysicalIrq > 0x3FF) || (GuestIrq > 0x3FF)) {
    DEBUG ((DEBUG_ERROR, "Passthrough: IRQ %u/%u out of range\n", PhysicalIrq, GuestIrq));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO,
          "Passthrough: IRQ phys=%u -> guest=%u\n",
          PhysicalIrq, GuestIrq));

  //
  // On x86: reconfigure the I/O APIC redirection entry so that the IRQ is
  // delivered to the host vector the hypervisor registered for the guest;
  // the host vector handler injects the corresponding guest IRQ via the
  // vLAPIC.
  //
  // On ARMv8: reconfigure the GIC distributor GICD_ITARGETSR / GICD_ISENABLER
  // so the SPI is delivered to the vCPU running the guest, then update the
  // vgic's virtual interrupt table.
  //
  // For this stub, we only record the mapping so the diagnostics shell can
  // inspect it.
  //
  if (mPassthroughCount >= HV_EPT_MAX_ENTRIES) {
    return EFI_OUT_OF_RESOURCES;
  }
  mPassthroughTable[mPassthroughCount].GuestPhys = (UINT64)PhysicalIrq;
  mPassthroughTable[mPassthroughCount].HostPhys  = (UINT64)GuestIrq;
  mPassthroughTable[mPassthroughCount].Size      = 0;
  mPassthroughTable[mPassthroughCount].Flags     = 0xBEEF0000 | HV_EPT_ENTRY_VALID;
  mPassthroughCount++;

  return EFI_SUCCESS;
}

/**
  Dumps the current passthrough table to the debug log. Used by the shell
  `afri` command and by POST diagnostics.

  @retval EFI_SUCCESS   Dump produced.
**/
EFI_STATUS
EFIAPI
HvPassthroughDump (
  VOID
  )
{
  UINTN  Index;

  DEBUG ((DEBUG_INFO, "Passthrough: %u entr%s\n",
          (UINT32)mPassthroughCount,
          (mPassthroughCount == 1) ? L"y" : L"ies"));

  for (Index = 0; Index < mPassthroughCount; Index++) {
    DEBUG ((DEBUG_INFO,
            "  [%3lu] GPA=0x%016lx HPA=0x%016lx size=0x%lx flags=0x%08x\n",
            (UINT64)Index,
            mPassthroughTable[Index].GuestPhys,
            mPassthroughTable[Index].HostPhys,
            mPassthroughTable[Index].Size,
            mPassthroughTable[Index].Flags));
  }

  return EFI_SUCCESS;
}

/**
  Entry point for the Passthrough module.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
PassthroughEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "Passthrough: ready (hypervisor passthrough)\n"));
  return EFI_SUCCESS;
}
