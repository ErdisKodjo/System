#ifndef AFROS_INFRASTRUCTURE_H
#define AFROS_INFRASTRUCTURE_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file afros_infra.h
 * @brief System-wide infrastructure and monitoring for AfriOS.
 */

void infra_log(const char *level, const char *component, const char *message);
afros_status_t infra_init(void);

#endif
