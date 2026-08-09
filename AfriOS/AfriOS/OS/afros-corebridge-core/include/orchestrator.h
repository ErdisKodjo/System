#ifndef AFROS_ORCHESTRATOR_H
#define AFROS_ORCHESTRATOR_H

#include "runtime_manager.h"

/**
 * @file orchestrator.h
 * @brief Central orchestrator for AfriOS unified execution environment.
 */

afros_status_t orchestrator_init(void);
afros_status_t orchestrator_run_app(const char *path);
afros_status_t orchestrator_monitor_system(void);

#endif
