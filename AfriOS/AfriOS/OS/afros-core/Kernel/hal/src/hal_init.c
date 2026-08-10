#include "afros_hal.h"

/**
 * @file hal_init.c
 * @brief Hardware Abstraction Layer initialization for AfriOS
 */

static afros_status_t hal_init_impl(void) {
    afros_status_t status;
    
    status = arch_console_ops.init(115200);
    if (status != AFROS_SUCCESS) {
        return status;
    }
    
    arch_console_ops.puts("[HAL] Console initialized\r\n");
    
    status = arch_cpu_ops.init();
    if (status != AFROS_SUCCESS) {
        return status;
    }
    arch_console_ops.puts("[HAL] CPU initialized\r\n");
    
    status = arch_memory_ops.init();
    if (status != AFROS_SUCCESS) {
        return status;
    }
    arch_console_ops.puts("[HAL] Memory initialized\r\n");
    
    status = arch_interrupt_ops.init();
    if (status != AFROS_SUCCESS) {
        return status;
    }
    arch_console_ops.puts("[HAL] Interrupts initialized\r\n");
    
    status = arch_timer_ops.init(1000);  /* 1000 Hz tick rate */
    if (status != AFROS_SUCCESS) {
        return status;
    }
    arch_console_ops.puts("[HAL] Timer initialized\r\n");
    
    status = arch_storage_ops.init();
    if (status != AFROS_SUCCESS) {
        return status;
    }
    arch_console_ops.puts("[HAL] Storage initialized\r\n");
    
    return AFROS_SUCCESS;
}

static afros_status_t hal_reset_impl(void) {
    arch_console_ops.puts("[HAL] System reset requested\r\n");
    /* CPU reset not yet implemented in port */
    while (1) { __asm__ volatile ("hlt"); }
}

static afros_status_t hal_shutdown_impl(void) {
    arch_console_ops.puts("[HAL] System shutdown requested\r\n");
    /* CPU shutdown not yet implemented in port */
    while (1) { __asm__ volatile ("hlt"); }
}

static afros_status_t hal_suspend_impl(void) {
    arch_console_ops.puts("[HAL] System suspend requested\r\n");
    /* CPU suspend not yet implemented in port */
    while (1) { __asm__ volatile ("hlt"); }
}

static afros_status_t hal_resume_impl(void) {
    arch_console_ops.puts("[HAL] System resume requested\r\n");
    /* CPU resume not yet implemented in port */
    return AFROS_ERROR_NOT_SUPPORTED;
}

static afros_status_t hal_get_power_source_impl(afros_power_source_t *source) {
    if (!source) return AFROS_ERROR_INVALID_PARAM;
    *source = AFROS_POWER_SOURCE_AC; 
    return AFROS_SUCCESS;
}

static afros_status_t hal_get_battery_level_impl(uint32_t *percentage) {
    if (!percentage) return AFROS_ERROR_INVALID_PARAM;
    *percentage = 100;
    return AFROS_SUCCESS;
}

hal_ops_t afros_hal_ops = {
    .init = hal_init_impl,
    .reset = hal_reset_impl,
    .shutdown = hal_shutdown_impl,
    .suspend = hal_suspend_impl,
    .resume = hal_resume_impl,
    .get_power_source = hal_get_power_source_impl,
    .get_battery_level = hal_get_battery_level_impl
};
