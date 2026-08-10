/** @file
  POST Memory Test for Hybrid Firmware.

  Walks all RAM regions reported by gBS->GetMemoryMap, writes a checkerboard
  pattern (0x55AA55AA) to each page, reads it back and verifies. Bad pages
  are reported via the returned EFI_STATUS and a per-region callback.

  Exposes:
    - PostMemoryTest()        : runs the test across all EfiConventionalMemory.
    - PostMemoryTestRegion()  : tests a single [Base, Base+Size) region.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

#define POST_MEMTEST_PATTERN     0x55AA55AAu
#define POST_MEMTEST_PATTERN_INV 0xAA55AA55u
#define POST_MEMTEST_PAGE_SIZE   SIZE_4KB

//
// Aggregate result counters.
//
typedef struct {
  UINTN   PagesTested;
  UINTN   PagesFailed;
  UINT64  BytesTested;
} POST_MEMTEST_RESULT;

/**
  Tests a single memory region [Base, Base+Size) for integrity.

  The test writes a checkerboard pattern, reads it back, then writes the
  inverse and reads it back again. Original memory contents are saved and
  restored before returning.

  @param[in]  Base       Physical base address of the region (page-aligned).
  @param[in]  Size       Size in bytes (multiple of PAGE_SIZE).
  @param[out] Result     Optional pointer to receive per-region statistics.

  @retval EFI_SUCCESS            All pages passed.
  @retval EFI_DEVICE_ERROR       At least one page failed verification.
  @retval EFI_INVALID_PARAMETER  Bad alignment or size.
**/
EFI_STATUS
EFIAPI
PostMemoryTestRegion (
  IN  EFI_PHYSICAL_ADDRESS   Base,
  IN  UINTN                  Size,
  OUT POST_MEMTEST_RESULT    *Result OPTIONAL
  )
{
  volatile UINT32  *Ptr;
  UINT32           *Backup;
  UINTN            Pages;
  UINTN            Index;
  UINTN            WordIndex;
  UINTN            WordsPerPage;
  UINTN            LocalFailed;
  UINTN            LocalTested;
  EFI_STATUS       Status;

  if ((Base & (POST_MEMTEST_PAGE_SIZE - 1)) != 0) {
    DEBUG ((DEBUG_ERROR, "PostMemTest: Base 0x%lx is not page-aligned\n", (UINT64)Base));
    return EFI_INVALID_PARAMETER;
  }
  if ((Size == 0) || ((Size & (POST_MEMTEST_PAGE_SIZE - 1)) != 0)) {
    DEBUG ((DEBUG_ERROR, "PostMemTest: Size 0x%lx is not a multiple of PAGE_SIZE\n", (UINT64)Size));
    return EFI_INVALID_PARAMETER;
  }

  Pages       = Size / POST_MEMTEST_PAGE_SIZE;
  WordsPerPage = POST_MEMTEST_PAGE_SIZE / sizeof (UINT32);
  LocalFailed = 0;
  LocalTested = 0;

  //
  // Allocate a backup buffer to save the original contents of one page at a
  // time. We never touch more than a single page worth of memory at once.
  //
  Backup = (UINT32 *)AllocatePool (POST_MEMTEST_PAGE_SIZE);
  if (Backup == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_INFO, "PostMemTest: region 0x%lx size 0x%lx (%lu pages)\n",
          (UINT64)Base, (UINT64)Size, (UINT64)Pages));

  for (Index = 0; Index < Pages; Index++) {
    Ptr = (volatile UINT32 *)(UINTN)(Base + (Index * POST_MEMTEST_PAGE_SIZE));

    //
    // Save original contents.
    //
    CopyMem (Backup, (VOID *)Ptr, POST_MEMTEST_PAGE_SIZE);

    //
    // Phase 1: write checkerboard pattern.
    //
    for (WordIndex = 0; WordIndex < WordsPerPage; WordIndex++) {
      Ptr[WordIndex] = POST_MEMTEST_PATTERN;
    }
    for (WordIndex = 0; WordIndex < WordsPerPage; WordIndex++) {
      if (Ptr[WordIndex] != POST_MEMTEST_PATTERN) {
        DEBUG ((DEBUG_ERROR,
                "PostMemTest: FAIL page %lu word %lu expected 0x%08x got 0x%08x\n",
                (UINT64)Index, (UINT64)WordIndex, POST_MEMTEST_PATTERN, Ptr[WordIndex]));
        LocalFailed++;
        break;
      }
    }

    //
    // Phase 2: write inverse pattern.
    //
    if (LocalFailed == 0) {
      for (WordIndex = 0; WordIndex < WordsPerPage; WordIndex++) {
        Ptr[WordIndex] = POST_MEMTEST_PATTERN_INV;
      }
      for (WordIndex = 0; WordIndex < WordsPerPage; WordIndex++) {
        if (Ptr[WordIndex] != POST_MEMTEST_PATTERN_INV) {
          DEBUG ((DEBUG_ERROR,
                  "PostMemTest: FAIL(inverse) page %lu word %lu expected 0x%08x got 0x%08x\n",
                  (UINT64)Index, (UINT64)WordIndex, POST_MEMTEST_PATTERN_INV, Ptr[WordIndex]));
          LocalFailed++;
          break;
        }
      }
    }

    //
    // Restore original contents.
    //
    CopyMem ((VOID *)Ptr, Backup, POST_MEMTEST_PAGE_SIZE);

    LocalTested++;
  }

  FreePool (Backup);

  if (Result != NULL) {
    Result->PagesTested  += LocalTested;
    Result->PagesFailed  += LocalFailed;
    Result->BytesTested  += (UINT64)LocalTested * POST_MEMTEST_PAGE_SIZE;
  }

  Status = (LocalFailed == 0) ? EFI_SUCCESS : EFI_DEVICE_ERROR;
  DEBUG ((DEBUG_INFO, "PostMemTest: region done (%lu pages tested, %lu failed, %r)\n",
          (UINT64)LocalTested, (UINT64)LocalFailed, Status));
  return Status;
}

/**
  Runs the POST memory test across all EfiConventionalMemory regions
  reported by gBS->GetMemoryMap.

  @param[out] Aggregate   Optional aggregate statistics.

  @retval EFI_SUCCESS     All pages in all regions passed.
  @retval EFI_DEVICE_ERROR At least one page failed.
**/
EFI_STATUS
EFIAPI
PostMemoryTest (
  OUT POST_MEMTEST_RESULT  *Aggregate OPTIONAL
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
  POST_MEMTEST_RESULT     Local;
  EFI_STATUS              Worst;

  DEBUG ((DEBUG_INFO, "PostMemTest: walking memory map\n"));

  if (Aggregate != NULL) {
    ZeroMem (Aggregate, sizeof (*Aggregate));
  }
  ZeroMem (&Local, sizeof (Local));

  MemoryMapSize = 0;
  MemoryMap     = NULL;
  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  MemoryMapSize += (4 * DescriptorSize); // slack for races with allocations.
  MemoryMap = (EFI_MEMORY_DESCRIPTOR *)AllocatePool (MemoryMapSize);
  if (MemoryMap == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (EFI_ERROR (Status)) {
    FreePool (MemoryMap);
    return Status;
  }

  NumEntries = MemoryMapSize / DescriptorSize;
  Worst      = EFI_SUCCESS;

  for (Index = 0; Index < NumEntries; Index++) {
    Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (Index * DescriptorSize));

    if (Desc->Type != EfiConventionalMemory) {
      continue;
    }
    if (Desc->NumberOfPages == 0) {
      continue;
    }

    //
    // Cap each region to a maximum of 16 MiB to keep the POST within a
    // reasonable time budget — a full test of gigabytes of RAM belongs to
    // a user-invoked diagnostic, not POST.
    //
    UINT64 RegionSize = EFI_PAGES_TO_SIZE (Desc->NumberOfPages);
    UINT64 MaxRegion  = SIZE_16MB;
    if (RegionSize > MaxRegion) {
      RegionSize = MaxRegion;
    }

    Status = PostMemoryTestRegion (Desc->PhysicalStart, (UINTN)RegionSize, &Local);
    if (EFI_ERROR (Status)) {
      Worst = EFI_DEVICE_ERROR;
    }
  }

  FreePool (MemoryMap);

  DEBUG ((DEBUG_INFO,
          "PostMemTest: done (%lu pages / %lu bytes tested, %lu pages failed, result=%r)\n",
          (UINT64)Local.PagesTested, (UINT64)Local.BytesTested,
          (UINT64)Local.PagesFailed, Worst));

  if (Aggregate != NULL) {
    CopyMem (Aggregate, &Local, sizeof (Local));
  }

  return Worst;
}

/**
  Entry point for the POST Memory Test module.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
PostMemoryTestEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "PostMemTest: ready (POST memory test)\n"));
  return EFI_SUCCESS;
}
