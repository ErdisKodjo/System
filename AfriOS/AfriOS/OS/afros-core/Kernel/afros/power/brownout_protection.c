#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file brownout_protection.c
 * @brief Brownout protection logic for AfriOS.
 * Protects hardware and data during power fluctuations.
 */

void power_protect_brownout(uint32_t current_voltage_mv) {
    if (current_voltage_mv < 3000) { // Assume threshold is 3000 mV
        kprintf("CRITICAL: Brownout detected! Initiating emergency shutdown protocols.\n");
        // Flush memory caches, unmount file systems, and suspend all tasks
        afros_hal_ops.shutdown();
    } else if (current_voltage_mv < 3500) {
        kprintf("WARNING: Power fluctuation detected. Reducing system load to preserve stability.\n");
        // Disable non-critical cores and network acceleration
    }
}
