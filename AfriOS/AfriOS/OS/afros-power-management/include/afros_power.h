#ifndef AFROS_POWER_H
#define AFROS_POWER_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file afros_power.h
 * @brief Power management interface for AfriOS.
 */

void power_monitor_battery(void);
afros_status_t power_init(void);

#endif
