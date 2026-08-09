#include "afros_hal.h"
#include <stdio.h>
#include <string.h>

/**
 * @file afros_scheduler.c
 * @brief Ordonnanceur central pour AfriOS.
 */

#define MAX_TASKS 64

static afros_task_t g_task_table[MAX_TASKS];
static uint32_t g_task_count = 0;
static int32_t g_current_task_idx = -1;

afros_status_t scheduler_init(void) {
    memset(g_task_table, 0, sizeof(g_task_table));
    g_task_count = 0;
    g_current_task_idx = -1;
    printf("[SCHED] Ordonnanceur initialis. Capacit : %d tches.\n", MAX_TASKS);
    return AFROS_SUCCESS;
}

afros_status_t scheduler_create_task(const char *name, uint32_t priority) {
    if (g_task_count >= MAX_TASKS) return AFROS_ERROR_NO_MEMORY;
    
    afros_task_t *task = &g_task_table[g_task_count];
    task->task_id = g_task_count + 1;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->priority = priority;
    task->state = AFROS_TASK_READY;
    task->cpu_affinity = 0xFF; // N'importe quel CPU par dfaut
    
    printf("[SCHED] Nouvelle tche cre : %s (ID: %u, Priorit: %u)\n", name, task->task_id, priority);
    g_task_count++;
    
    return AFROS_SUCCESS;
}

void scheduler_schedule_next(void) {
    if (g_task_count == 0) return;
    
    // Simple algorithme Round-Robin avec priorit
    uint32_t next_idx = (g_current_task_idx + 1) % g_task_count;
    
    // Marquer l'ancienne tche comme READY si elle tournait
    if (g_current_task_idx >= 0 && g_task_table[g_current_task_idx].state == AFROS_TASK_RUNNING) {
        g_task_table[g_current_task_idx].state = AFROS_TASK_READY;
    }
    
    g_current_task_idx = next_idx;
    afros_task_t *next_task = &g_task_table[g_current_task_idx];
    next_task->state = AFROS_TASK_RUNNING;
    
    printf("[SCHED] Switch vers tche : %s (ID: %u)\n", next_task->name, next_task->task_id);
}
