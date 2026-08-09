#include "../include/afros_infra.h"
#include <stdio.h>
#include <time.h>

/**
 * @file infra_core.c
 * @brief Implementation of the system-wide infrastructure for AfriOS.
 */

afros_status_t infra_init(void) {
    printf("[INFRA] Initializing system-wide monitoring and logging...\n");
    return AFROS_SUCCESS;
}

void infra_log(const char *level, const char *component, const char *message) {
    time_t now;
    time(&now);
    char *timestamp = ctime(&now);
    timestamp[24] = '\0'; // Remove newline

    printf("[%s] [%s] [%s]: %s\n", timestamp, level, component, message);
}
