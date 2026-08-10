#ifndef AFROS_HAL_H
#define AFROS_HAL_H

#include "afros_types.h"
#include "cpu_abstraction.h"
#include "memory_abstraction.h"
#include "device_abstraction.h"
#include "interrupt_abstraction.h"
#include "timer_abstraction.h"
#include "console_abstraction.h"
#include "storage_abstraction.h"

/**
 * @file afros_hal.h
 * @brief Main Hardware Abstraction Layer interface for AfriOS.
 *
 * MMU: pas d'en-tête séparé — gérée par memory_abstraction.h (map/unmap sur
 * arch_memory_ops), la MMU étant un aspect de la gestion mémoire et non un
 * périphérique distinct.
 *
 * Toutes ces tables d'ops (arch_cpu_ops, arch_memory_ops, arch_interrupt_ops,
 * arch_timer_ops, arch_console_ops, arch_storage_ops) sont fournies par le
 * port actif — voir Kernel/ports/. hal/src/ ne fait que les agréger et les
 * exposer, sans jamais coder en dur une architecture particulière.
 */

typedef struct {
    afros_status_t (*init)(void);
    afros_status_t (*reset)(void);
    afros_status_t (*shutdown)(void);
    afros_status_t (*suspend)(void);
    afros_status_t (*resume)(void);
    
    // Power awareness
    afros_status_t (*get_power_source)(afros_power_source_t *source);
    afros_status_t (*get_battery_level)(uint32_t *percentage);
} hal_ops_t;

extern hal_ops_t afros_hal_ops;

#endif // AFROS_HAL_H
