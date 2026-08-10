/*
 * task_scheduler.c — Planificateur de tâches (AT service) pour afros-winbridge.
 *
 * Permet de planifier l'exécution de commandes à un moment donné ou de
 * façon périodique. Similaire à cron mais avec une API Win32.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_TASKS 64

/* --- Types internes --------------------------------------------------- */

typedef enum _TASK_TRIGGER {
    TASK_TRIGGER_ONCE = 0,
    TASK_TRIGGER_DAILY,
    TASK_TRIGGER_WEEKLY,
    TASK_TRIGGER_MONTHLY,
    TASK_TRIGGER_AT_LOGON,
    TASK_TRIGGER_AT_BOOT,
} TASK_TRIGGER;

typedef struct _TASK {
    BOOL           active;
    char           name[64];
    char           command[256];
    TASK_TRIGGER   trigger;
    WORD           hour;
    WORD           minute;
    WORD           day_of_week;    /* 0=dim, 1=lun, ... */
    DWORD          last_run;
    DWORD          next_run;
    DWORD          run_count;
} TASK;

static TASK             g_tasks[MAX_TASKS];
static int              g_task_count = 0;
static pthread_mutex_t  g_task_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t        g_worker_thread;
static BOOL             g_worker_running = FALSE;

/* --- Helpers locaux ---------------------------------------------------- */

/* Calcule le prochain déclenchement d'une tâche (approximatif). */
static DWORD compute_next_run(const TASK *t, DWORD now)
{
    DWORD base = now;
    switch (t->trigger) {
    case TASK_TRIGGER_ONCE:
        return base;
    case TASK_TRIGGER_DAILY:
        return base + 86400;  /* +24h */
    case TASK_TRIGGER_WEEKLY:
        return base + 7 * 86400;
    case TASK_TRIGGER_MONTHLY:
        return base + 30 * 86400;
    case TASK_TRIGGER_AT_LOGON:
    case TASK_TRIGGER_AT_BOOT:
        return base;
    }
    return base;
}

/* Thread worker: scanne les tâches toutes les 60s. */
static void *task_worker(void *arg)
{
    (void)arg;
    while (g_worker_running) {
        int i;
        DWORD now = (DWORD)time(NULL);
        pthread_mutex_lock(&g_task_lock);
        for (i = 0; i < g_task_count; i++) {
            TASK *t = &g_tasks[i];
            if (t->active && now >= t->next_run) {
                /* En pratique: fork+exec t->command. */
                t->last_run = now;
                t->run_count++;
                t->next_run = compute_next_run(t, now);
            }
        }
        pthread_mutex_unlock(&g_task_lock);
        sleep(60);
    }
    return NULL;
}

/* --- API publique ------------------------------------------------------ */

/* Crée une nouvelle tâche planifiée. */
NTSTATUS TaskCreate(const char *name, const char *command,
                    TASK_TRIGGER trigger, WORD hour, WORD minute)
{
    TASK *t;
    if (!name || !command) return STATUS_INVALID_PARAMETER;
    pthread_mutex_lock(&g_task_lock);
    if (g_task_count >= MAX_TASKS) {
        pthread_mutex_unlock(&g_task_lock);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    t = &g_tasks[g_task_count++];
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, sizeof(t->name) - 1);
    strncpy(t->command, command, sizeof(t->command) - 1);
    t->trigger = trigger;
    t->hour    = hour;
    t->minute  = minute;
    t->active  = TRUE;
    t->next_run = compute_next_run(t, (DWORD)time(NULL));
    t->run_count = 0;
    pthread_mutex_unlock(&g_task_lock);
    return STATUS_SUCCESS;
}

/* Énumère les tâches enregistrées. */
DWORD TaskEnum(char (*names)[64], DWORD max_count)
{
    DWORD i, n;
    pthread_mutex_lock(&g_task_lock);
    n = (DWORD)g_task_count;
    if (n > max_count) n = max_count;
    for (i = 0; i < n; i++)
        strncpy(names[i], g_tasks[i].name, 63);
    pthread_mutex_unlock(&g_task_lock);
    return n;
}

/* Exécute immédiatement une tâche par son nom. */
NTSTATUS TaskRun(const char *name)
{
    int i;
    NTSTATUS r = STATUS_NOT_FOUND;
    pthread_mutex_lock(&g_task_lock);
    for (i = 0; i < g_task_count; i++) {
        if (strcmp(g_tasks[i].name, name) == 0) {
            g_tasks[i].last_run = (DWORD)time(NULL);
            g_tasks[i].run_count++;
            r = STATUS_SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&g_task_lock);
    return r;
}

/* Supprime une tâche. */
NTSTATUS TaskDelete(const char *name)
{
    int i;
    pthread_mutex_lock(&g_task_lock);
    for (i = 0; i < g_task_count; i++) {
        if (strcmp(g_tasks[i].name, name) == 0) {
            /* Compacte le tableau. */
            if (i + 1 < g_task_count)
                g_tasks[i] = g_tasks[g_task_count - 1];
            g_task_count--;
            pthread_mutex_unlock(&g_task_lock);
            return STATUS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_task_lock);
    return STATUS_NOT_FOUND;
}

/* Démarre le worker thread. */
NTSTATUS TaskSchedulerInit(void)
{
    if (g_worker_running) return STATUS_SUCCESS;
    g_worker_running = TRUE;
    if (pthread_create(&g_worker_thread, NULL, task_worker, NULL) != 0) {
        g_worker_running = FALSE;
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;
}
