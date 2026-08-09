/** @file
  Generic implementation of the CPU HAL Library.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include "CpuHal.h"
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

EFI_STATUS
EFIAPI
CpuHalInitialize (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "CpuHalInitialize: Initializing Hybrid CPU HAL\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CpuHalGetInfo (
  IN  UINT32           ProcessorId,
  OUT HYBRID_CPU_INFO  *Info
  )
{
  if (Info == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Info->ProcessorId = ProcessorId;

  //
  // Topologie simulée, différenciée par architecture (étape 3) : la version
  // précédente utilisait un seul chemin "ProcessorId < 4 => big" pour toutes
  // les architectures, y compris RISC-V où big.LITTLE n'est pas un concept
  // standard et IA32/X64 où le SMT ne double pas nécessairement les threads
  // de la même façon selon P-core/E-core.
  //
#if defined (MDE_CPU_AARCH64)
  // big.LITTLE : 4 cœurs "big" (0-3) + 4 "LITTLE" (4-7), pas de SMT sur la
  // majorité des cœurs ARM grand public.
  Info->CoreCount        = 8;
  Info->ThreadCount       = 8;
  Info->MaxFrequencyMHz   = 3200;
  Info->IsBigCore         = (ProcessorId < 4);
  Info->CurrentFrequencyMHz = Info->IsBigCore ? 2800 : 1800;

#elif defined (MDE_CPU_X64) || defined (MDE_CPU_IA32)
  // P-core (0-3, avec Hyper-Threading) + E-core (4-7, sans HT) façon Intel.
  Info->CoreCount        = 8;
  Info->ThreadCount       = 16;
  Info->MaxFrequencyMHz   = 3600;
  Info->IsBigCore         = (ProcessorId < 4);
  Info->CurrentFrequencyMHz = Info->IsBigCore ? 3600 : 2400;

#elif defined (MDE_CPU_RISCV64)
  // Pas de notion big.LITTLE standardisée : harts homogènes par défaut.
  Info->CoreCount        = 4;
  Info->ThreadCount       = 4;
  Info->MaxFrequencyMHz   = 1400;
  Info->IsBigCore         = TRUE;
  Info->CurrentFrequencyMHz = 1400;

#else
  // Repli générique : un seul cœur, pas d'hypothèse sur la fréquence.
  Info->CoreCount        = 1;
  Info->ThreadCount       = 1;
  Info->MaxFrequencyMHz   = 0;
  Info->IsBigCore         = TRUE;
  Info->CurrentFrequencyMHz = 0;
#endif

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CpuHalSetFrequency (
  IN  UINT32  ProcessorId,
  IN  UINT32  FrequencyMHz
  )
{
  DEBUG ((DEBUG_INFO, "CpuHalSetFrequency: Setting CPU %d frequency to %d MHz\n", ProcessorId, FrequencyMHz));
  // In a real implementation, this would write to MSRs or PMIC registers.
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CpuHalSleepCore (
  IN  UINT32  ProcessorId
  )
{
  DEBUG ((DEBUG_INFO, "CpuHalSleepCore: CPU %d entering sleep state\n", ProcessorId));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CpuHalWakeupCore (
  IN  UINT32  ProcessorId
  )
{
  DEBUG ((DEBUG_INFO, "CpuHalWakeupCore: CPU %d waking up\n", ProcessorId));
  return EFI_SUCCESS;
}
