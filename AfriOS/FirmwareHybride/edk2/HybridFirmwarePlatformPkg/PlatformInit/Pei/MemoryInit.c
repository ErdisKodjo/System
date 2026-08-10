/** @file
  Memory Initialization PEI Module.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiPei.h>
#include <Library/PeimEntryPoint.h>
#include <Library/PeiServicesLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>

/**
  Initializes the system memory and publishes Resource Descriptor HOBs.

  @param[in]  FileHandle  Handle of the file being invoked.
  @param[in]  PeiServices Describes the list of possible PEI Services.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
MemoryInitEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_PHYSICAL_ADDRESS  MemoryBase = 0x0;
  UINT64                MemorySize = 0x20000000; // 512MB default

  DEBUG ((DEBUG_INFO, "MemoryInit: Initializing system memory...\n"));

  // In a real platform, we would read the memory controller or SPD to detect DRAM.
  // For this hybrid platform, we assume 512MB for simulation.

  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    (EFI_RESOURCE_ATTRIBUTE_PRESENT |
     EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
     EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
     EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEable |
     EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
     EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
     EFI_RESOURCE_ATTRIBUTE_TESTED),
    MemoryBase,
    MemorySize
    );

  BuildMemoryAllocationHob (
    MemoryBase,
    MemorySize,
    EfiConventionalMemory
    );

  DEBUG ((DEBUG_INFO, "MemoryInit: 512MB RAM initialized at 0x%lx\n", MemoryBase));

  return EFI_SUCCESS;
}
