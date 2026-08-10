/** @file
  CPU Abstraction Layer for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#ifndef CPU_HAL_H_
#define CPU_HAL_H_

#include <Uefi.h>
#include <Library/BaseLib.h>

typedef struct {
  UINT32 ProcessorId;
  UINT32 CoreCount;
  UINT32 ThreadCount;
  UINT32 CurrentFrequencyMHz;
  UINT32 MaxFrequencyMHz;
  BOOLEAN IsBigCore;
} HYBRID_CPU_INFO;

/**
  Initializes the CPU HAL.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
CpuHalInitialize (
  VOID
  );

/**
  Gets information about a specific CPU core.

  @param[in]  ProcessorId  The ID of the processor to query.
  @param[out] Info         Pointer to the info structure to populate.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
CpuHalGetInfo (
  IN  UINT32           ProcessorId,
  OUT HYBRID_CPU_INFO  *Info
  );

/**
  Sets the frequency of a specific CPU core.

  @param[in]  ProcessorId  The ID of the processor.
  @param[in]  FrequencyMHz The target frequency in MHz.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
CpuHalSetFrequency (
  IN  UINT32  ProcessorId,
  IN  UINT32  FrequencyMHz
  );

/**
  Puts a CPU core into a low-power sleep state.

  @param[in]  ProcessorId  The ID of the processor.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
CpuHalSleepCore (
  IN  UINT32  ProcessorId
  );

/**
  Wakes up a CPU core from a sleep state.

  @param[in]  ProcessorId  The ID of the processor.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
CpuHalWakeupCore (
  IN  UINT32  ProcessorId
  );

#endif
