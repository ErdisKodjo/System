/** @file
  Minimal Hypervisor Initialization for Hybrid Firmware.
  Supports x64 VT-x.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

/**
  Checks if VT-x is supported and enabled on the current CPU.

  @return EFI_SUCCESS if supported and enabled.
**/
EFI_STATUS
CheckVtxSupport (
  VOID
  )
{
  UINT32  Eax, Ebx, Ecx, Edx;

  // 1. Check CPUID Leaf 1, ECX bit 5 (VMX)
  AsmCpuid (1, &Eax, &Ebx, &Ecx, &Edx);
  if (!(Ecx & BIT5)) {
    DEBUG ((DEBUG_ERROR, "Hypervisor: VT-x not supported by CPU\n"));
    return EFI_UNSUPPORTED;
  }

  // 2. Check IA32_FEATURE_CONTROL MSR (0x3A)
  // Bit 0: Lock, Bit 2: Enable VMX outside SMX
  #if defined(__x86_64__)
    UINT64 FeatureControl = AsmReadMsr64 (0x3A);
    if (!(FeatureControl & BIT0) || !(FeatureControl & BIT2)) {
      DEBUG ((DEBUG_ERROR, "Hypervisor: VT-x disabled in BIOS/MSR (0x3A = %lx)\n", FeatureControl));
      return EFI_ACCESS_DENIED;
    }
  #endif

  return EFI_SUCCESS;
}

/**
  Initializes the Virtual Machine Control Structure (VMCS).

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
InitializeVmcs (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "Hypervisor: Initializing VMCS...\n"));

  // 1. Allocate VMCS region (4KB, aligned)
  // 2. Set Revision ID
  // 3. VMPTRLD
  // 4. Setup Host/Guest state, Controls, etc.

  DEBUG ((DEBUG_INFO, "Hypervisor: VMCS initialized (Simulated)\n"));
  return EFI_SUCCESS;
}

/**
  Entry point for Minimal Hypervisor.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
HypervisorInit (
  VOID
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "Hypervisor: Starting initialization...\n"));

  Status = CheckVtxSupport ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitializeVmcs ();
  return Status;
}
