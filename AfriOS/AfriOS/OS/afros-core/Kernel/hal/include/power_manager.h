#ifndef AFROS_POWER_MANAGER_H
#define AFROS_POWER_MANAGER_H

#include "afros_types.h"

/**
 * @file power_manager.h
 * @brief Power management interface for AfriOS HAL.
 */

typedef enum {
    POWER_STATE_S0 = 0,  // Working
    POWER_STATE_S1,      // Sleep - CPU stopped, cache maintained
    POWER_STATE_S2,      // Sleep - CPU off, cache flushed
    POWER_STATE_S3,      // Suspend to RAM
    POWER_STATE_S4,      // Hibernate (Suspend to Disk)
    POWER_STATE_S5,      // Soft Off
    POWER_STATE_G3       // Mechanical Off
} power_state_t;

typedef enum {
    POWER_EVENT_AC_CONNECTED = 0,
    POWER_EVENT_AC_DISCONNECTED,
    POWER_EVENT_BATTERY_LOW,
    POWER_EVENT_BATTERY_CRITICAL,
    POWER_EVENT_LID_CLOSED,
    POWER_EVENT_LID_OPENED,
    POWER_EVENT_BUTTON_PRESSED
} power_event_t;

typedef void (*power_event_handler_t)(power_event_t event);

// Initialization
afros_status_t power_manager_init(void);

// Power state control
afros_status_t power_set_state(power_state_t state);
power_state_t power_get_current_state(void);

// Event handling
afros_status_t power_register_event_handler(power_event_t event, power_event_handler_t handler);
afros_status_t power_unregister_event_handler(power_event_t event);

// Battery monitoring (if available)
uint32_t power_get_battery_percentage(void);
bool power_is_on_ac_power(void);

// CPU power management
afros_status_t power_enable_cpu_idle_states(bool enable);
afros_status_t power_set_performance_mode(uint32_t level); // 0=min, 100=max

#endif // AFROS_POWER_MANAGER_H
