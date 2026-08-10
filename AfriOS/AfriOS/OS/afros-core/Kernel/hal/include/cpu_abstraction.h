#ifndef CPU_ABSTRACTION_H
#define CPU_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file cpu_abstraction.h
 * @brief CPU abstraction layer for AfriOS.
 */

typedef struct {
    afros_status_t (*init)(void);
    afros_status_t (*get_info)(uint32_t cpu_id, afros_cpu_info_t *info);
    afros_status_t (*set_frequency)(uint32_t cpu_id, uint32_t freq_mhz);
    afros_status_t (*sleep_core)(uint32_t cpu_id);
    afros_status_t (*wakeup_core)(uint32_t cpu_id);
    afros_status_t (*migrate_task)(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id);
} cpu_ops_t;

extern cpu_ops_t arch_cpu_ops;

#endif // CPU_ABSTRACTION_H
