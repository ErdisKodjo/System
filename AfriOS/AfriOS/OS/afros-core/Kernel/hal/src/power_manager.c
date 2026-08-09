/**
 * @file power_manager.c
 * @brief Power management implementation for AfriOS HAL (x86_64).
 */

#include "afros_types.h"
#include "../include/power_manager.h"
#include <stdint.h>
#include <stdbool.h>

// Internal state
static power_state_t g_current_state = POWER_STATE_S0;
static power_event_handler_t g_event_handlers[8] = {0};
static bool g_cpu_idle_enabled = true;
static uint32_t g_performance_mode = 100;

// ACPI ports (to be implemented in port-x86_64)
extern void acpi_write_sleep_state(uint8_t sleep_type);
extern uint8_t acpi_read_pm_status(void);

static void power_handle_event(power_event_t event) {
    if (event < 8 && g_event_handlers[event] != NULL) {
        g_event_handlers[event](event);
    }
}

afros_status_t power_manager_init(void) {
    // Initialize power management subsystem
    g_current_state = POWER_STATE_S0;
    g_cpu_idle_enabled = true;
    g_performance_mode = 100;
    
    // Clear all event handlers
    for (int i = 0; i < 8; i++) {
        g_event_handlers[i] = NULL;
    }
    
    // TODO: Initialize ACPI interface
    // TODO: Register for SCI (System Control Interrupt)
    
    return AFROS_SUCCESS;
}

afros_status_t power_set_state(power_state_t state) {
    if (state > POWER_STATE_G3) {
        return AFROS_ERR_INVALID_PARAM;
    }
    
    switch (state) {
        case POWER_STATE_S0:
            // Already in S0 or waking up
            g_current_state = POWER_STATE_S0;
            break;
            
        case POWER_STATE_S1:
        case POWER_STATE_S2:
        case POWER_STATE_S3:
            // Enter sleep state via ACPI
            // acpi_write_sleep_state(state);
            // __asm__ volatile ("hlt");
            g_current_state = state;
            break;
            
        case POWER_STATE_S4:
            // Hibernate - save memory to disk and shutdown
            // TODO: Implement hibernation
            g_current_state = state;
            break;
            
        case POWER_STATE_S5:
        case POWER_STATE_G3:
            // Shutdown
            // acpi_write_sleep_state(5);
            g_current_state = state;
            break;
            
        default:
            return AFROS_ERR_INVALID_PARAM;
    }
    
    return AFROS_SUCCESS;
}

power_state_t power_get_current_state(void) {
    return g_current_state;
}

afros_status_t power_register_event_handler(power_event_t event, power_event_handler_t handler) {
    if (event >= 8 || handler == NULL) {
        return AFROS_ERR_INVALID_PARAM;
    }
    
    g_event_handlers[event] = handler;
    return AFROS_SUCCESS;
}

afros_status_t power_unregister_event_handler(power_event_t event) {
    if (event >= 8) {
        return AFROS_ERR_INVALID_PARAM;
    }
    
    g_event_handlers[event] = NULL;
    return AFROS_SUCCESS;
}

uint32_t power_get_battery_percentage(void) {
    // TODO: Read from ACPI battery interface
    // For now, return 100% (assuming AC power)
    return 100;
}

bool power_is_on_ac_power(void) {
    // TODO: Read from ACPI power status
    // For now, assume always on AC
    return true;
}

afros_status_t power_enable_cpu_idle_states(bool enable) {
    g_cpu_idle_enabled = enable;
    // TODO: Configure CPU C-states via MSR
    return AFROS_SUCCESS;
}

afros_status_t power_set_performance_mode(uint32_t level) {
    if (level > 100) {
        level = 100;
    }
    
    g_performance_mode = level;
    // TODO: Configure CPU P-states / frequency scaling
    return AFROS_SUCCESS;
}

// Architecture-specific idle function
void arch_idle(void) {
    if (g_cpu_idle_enabled) {
        __asm__ volatile ("hlt" ::: "memory");
    }
}