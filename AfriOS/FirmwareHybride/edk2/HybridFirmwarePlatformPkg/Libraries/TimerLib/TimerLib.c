/** @file
  Timer Library implementation for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <Base.h>
#include <Library/TimerLib.h>
#include <Library/DebugLib.h>

/**
  Stalls the CPU for at least the given number of microseconds.

  @param[in]  MicroSeconds  The minimum number of microseconds to stall.

  @return The actual number of microseconds stalled.
**/
UINTN
EFIAPI
MicroSecondDelay (
  IN      UINTN                     MicroSeconds
  )
{
  // In a real implementation, this would read a hardware timer (e.g., HPET, ARM Generic Timer).
  // For simulation, we use a simple loop or a nop.
  for (volatile UINTN i = 0; i < MicroSeconds * 100; i++) {
    #if defined(__x86_64__)
      __asm__ __volatile__("pause");
    #elif defined(__aarch64__)
      __asm__ __volatile__("yield");
    #endif
    // RISC-V : pas d'instruction "pause" universelle sans dépendre de
    // l'extension Zihintpause (pas garantie présente) - boucle nue, correcte
    // mais sans l'optimisation d'attente donnée aux deux autres archs.
  }
  return MicroSeconds;
}

/**
  Stalls the CPU for at least the given number of nanoseconds.

  @param[in]  NanoSeconds  The minimum number of nanoseconds to stall.

  @return The actual number of nanoseconds stalled.
**/
UINTN
EFIAPI
NanoSecondDelay (
  IN      UINTN                     NanoSeconds
  )
{
  return MicroSecondDelay (NanoSeconds / 1000 + 1);
}

/**
  Retrieves the current value of a 64-bit free running performance counter.

  @return The current value of the performance counter.
**/
UINT64
EFIAPI
GetPerformanceCounter (
  VOID
  )
{
  // Étape 4 : remplace le stub "return 0" par le compteur matériel réel de
  // chaque architecture (tache.md, section "Finalisation des Stubs").
#if defined(__x86_64__) || defined(__i386__)
  UINT32 Lo, Hi;
  __asm__ __volatile__ ("rdtsc" : "=a" (Lo), "=d" (Hi));
  return ((UINT64)Hi << 32) | Lo;

#elif defined(__aarch64__)
  UINT64 Counter;
  __asm__ __volatile__ ("mrs %0, cntpct_el0" : "=r" (Counter));
  return Counter;

#elif defined(__riscv) && (__riscv_xlen == 64)
  UINT64 Counter;
  __asm__ __volatile__ ("rdtime %0" : "=r" (Counter));
  return Counter;

#else
  // Repli générique (hôte de build/simulation sans compteur matériel connu) :
  // compteur logiciel monotone, pas une mesure de temps réel.
  static UINT64  SoftwareCounter = 0;
  return SoftwareCounter++;
#endif
}

/**
  Retrieves the 64-bit performance counter characteristics.

  @param[out] StartValue  The value the performance counter starts with.
  @param[out] EndValue    The value the performance counter ends with.

  @return The performance counter characteristics.
**/
UINT64
EFIAPI
GetPerformanceCounterProperties (
  OUT      UINT64                    *StartValue,  OPTIONAL
  OUT      UINT64                    *EndValue     OPTIONAL
  )
{
  // NOTE : fréquence non asservie au compteur réellement lu par
  // GetPerformanceCounter (TSC/CNTPCT_EL0/rdtime ont chacun leur propre
  // fréquence, dépendante du matériel réel). 1 MHz reste un placeholder tant
  // qu'aucune calibration (CPUID, CNTFRQ_EL0, timebase-frequency du DT)
  // n'est branchée ici.
  if (StartValue != NULL) *StartValue = 0;
  if (EndValue != NULL) *EndValue = 0xFFFFFFFFFFFFFFFFULL;
  return 1000000; // 1MHz frequency
}

/**
  Converts performance counter value to microseconds.

  @param[in]  CounterValue  Performance counter value to convert.

  @return The number of microseconds.
**/
UINT64
EFIAPI
GetTimeInNanoSecond (
  IN      UINT64                    CounterValue
  )
{
  return CounterValue * 1000;
}
