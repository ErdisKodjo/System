#ifndef AFROS_TYPES_H
#define AFROS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file afros_types.h
 * @brief Common kernel types for AfriOS core.
 */

typedef uint32_t afros_status_t;

#define AFROS_SUCCESS           0
#define AFROS_ERROR             1
#define AFROS_ERROR_INVALID_PARAM 2
#define AFROS_ERROR_NO_MEMORY   3
#define AFROS_ERROR_NOT_SUPPORTED 4
#define AFROS_ERROR_TIMEOUT     5
#define AFROS_ERROR_INVALID_STATE 6
#define AFROS_ERROR_NOT_FOUND   7
#define AFROS_ERROR_PERMISSION  8
#define AFROS_ERROR_BUSY        9
#define AFROS_ERROR_IO          10

typedef uint64_t afros_phys_addr_t;
typedef uint64_t afros_virt_addr_t;
typedef size_t   afros_size_t;

typedef struct {
    uint32_t core_id;
    uint32_t cluster_id;
    bool is_big;
    uint32_t current_freq_mhz;
} afros_cpu_info_t;

typedef enum {
    AFROS_TASK_READY,
    AFROS_TASK_RUNNING,
    AFROS_TASK_WAITING,
    AFROS_TASK_TERMINATED
} afros_task_state_t;

typedef struct {
    uint32_t task_id;
    char name[32];
    uint32_t priority;
    afros_task_state_t state;
    afros_virt_addr_t stack_pointer;
    uint32_t cpu_affinity; // Targeted CPU ID, 0xFF for any
    uint64_t cpu_time_ms;
} afros_task_t;

typedef enum {
    AFROS_POWER_SOURCE_BATTERY,
    AFROS_POWER_SOURCE_AC,
    AFROS_POWER_SOURCE_SOLAR,
    AFROS_POWER_SOURCE_UNKNOWN
} afros_power_source_t;

#endif // AFROS_TYPES_H
