#include "../include/runtime_manager.h"
#include <stdio.h>
#include <string.h>

/**
 * @file runtime_registry.c
 * @brief Registry for platform-specific runtime managers in AfriOS.
 */

#define MAX_RUNTIMES 10

static struct {
    afros_runtime_type_t type;
    runtime_ops_t *ops;
} g_runtime_registry[MAX_RUNTIMES];

static uint32_t g_runtime_count = 0;
static bool g_runtime_initialized = false;

afros_status_t runtime_register_manager(afros_runtime_type_t type, runtime_ops_t *ops) {
    if (g_runtime_count >= MAX_RUNTIMES) return AFROS_ERROR_NO_MEMORY;
    if (!ops) return AFROS_ERROR_INVALID_PARAM;
    
    g_runtime_registry[g_runtime_count].type = type;
    g_runtime_registry[g_runtime_count].ops = ops;
    g_runtime_count++;
    
    printf("AfriOS Runtime Registry: Registered runtime type %d.\n", type);
    return AFROS_SUCCESS;
}

afros_status_t runtime_init(void) {
    if (g_runtime_initialized) return AFROS_SUCCESS;
    
    printf("AfriOS Runtime Registry: Initializing all registered runtime managers...\n");
    for (uint32_t i = 0; i < g_runtime_count; i++) {
        if (g_runtime_registry[i].ops && g_runtime_registry[i].ops->initialize) {
            g_runtime_registry[i].ops->initialize();
        }
    }
    
    g_runtime_initialized = true;
    return AFROS_SUCCESS;
}
