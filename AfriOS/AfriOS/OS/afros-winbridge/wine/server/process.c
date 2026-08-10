/*
 * process.c — Gestion des processus clients du wineserver.
 *
 * Maintient la liste des processus Win32 spawnés, leur table de handles,
 * leur thread list et leur window station associée.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_PROCESSES 256
#define MAX_THREADS_PER_PROCESS 64
#define MAX_HANDLES_PER_PROCESS 512

/* --- Structures internes --------------------------------------------- */

typedef struct _WIN_THREAD {
    pthread_t tid;
    DWORD     thread_id;
    BOOL      running;
    void     *entry;
} WIN_THREAD;

typedef struct _WIN_PROCESS {
    pid_t       pid;
    DWORD       process_id;
    char        image_path[256];
    char        cmdline[1024];
    BOOL        running;
    HANDLE      handle;
    WIN_THREAD  threads[MAX_THREADS_PER_PROCESS];
    int         thread_count;
    void       *handle_table[MAX_HANDLES_PER_PROCESS];
    char        window_station[64];
    DWORD       exit_code;
} WIN_PROCESS;

static WIN_PROCESS       g_processes[MAX_PROCESSES];
static int               g_process_count = 0;
static pthread_mutex_t   g_proc_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- Helpers locaux ---------------------------------------------------- */

static WIN_PROCESS *find_by_pid(DWORD pid)
{
    int i;
    for (i = 0; i < g_process_count; i++)
        if (g_processes[i].process_id == pid) return &g_processes[i];
    return NULL;
}

static WIN_PROCESS *alloc_slot(void)
{
    if (g_process_count >= MAX_PROCESSES) return NULL;
    return &g_processes[g_process_count++];
}

/* --- API publique ------------------------------------------------------ */

/* Crée un nouveau processus Win32 (fork + image_path). */
HANDLE ProcessCreate(const char *image_path, const char *cmdline)
{
    WIN_PROCESS *p;
    pid_t pid;
    if (!image_path) return NULL;
    pthread_mutex_lock(&g_proc_lock);
    p = alloc_slot();
    if (!p) { pthread_mutex_unlock(&g_proc_lock); return NULL; }
    memset(p, 0, sizeof(*p));
    strncpy(p->image_path, image_path, sizeof(p->image_path) - 1);
    strncpy(p->cmdline, cmdline ? cmdline : "", sizeof(p->cmdline) - 1);
    strncpy(p->window_station, "WinSta0", sizeof(p->window_station) - 1);
    p->process_id = (DWORD)(g_process_count + 1000);
    pthread_mutex_unlock(&g_proc_lock);

    pid = fork();
    if (pid == 0) {
        /* Child: exécute l'image PE via wine_loader. */
        execl("/usr/lib/wine/wine_loader", "wine_loader", image_path, cmdline, NULL);
        _exit(127);
    } else if (pid < 0) {
        return NULL;
    }
    p->pid = pid;
    p->running = TRUE;
    p->handle = (HANDLE)(LONG_PTR)(p->process_id);
    return p->handle;
}

/* Récupère un processus par son pid. */
WIN_PROCESS *ProcessGetByPid(DWORD pid)
{
    return find_by_pid(pid);
}

/* Termine un processus. */
NTSTATUS ProcessTerminate(HANDLE h, NTSTATUS exit_code)
{
    DWORD pid = (DWORD)(LONG_PTR)h;
    WIN_PROCESS *p;
    pthread_mutex_lock(&g_proc_lock);
    p = find_by_pid(pid);
    if (!p) { pthread_mutex_unlock(&g_proc_lock); return STATUS_NOT_FOUND; }
    if (p->running) {
        p->exit_code = (DWORD)exit_code;
        p->running = FALSE;
        /* kill(p->pid, SIGTERM); */ /* désactivé: pas de signaux en sandbox */
    }
    pthread_mutex_unlock(&g_proc_lock);
    return STATUS_SUCCESS;
}

/* Ajoute un thread à un processus. */
NTSTATUS ProcessAddThread(DWORD pid, pthread_t tid, void *entry)
{
    WIN_PROCESS *p;
    pthread_mutex_lock(&g_proc_lock);
    p = find_by_pid(pid);
    if (!p || p->thread_count >= MAX_THREADS_PER_PROCESS) {
        pthread_mutex_unlock(&g_proc_lock);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    {
        WIN_THREAD *t = &p->threads[p->thread_count++];
        t->tid = tid;
        t->entry = entry;
        t->running = TRUE;
        t->thread_id = (DWORD)(p->thread_count + 2000);
    }
    pthread_mutex_unlock(&g_proc_lock);
    return STATUS_SUCCESS;
}

/* Énumère les processus actifs (callback). */
void ProcessEnum(void (*cb)(DWORD pid, const char *image, void *ctx), void *ctx)
{
    int i;
    if (!cb) return;
    pthread_mutex_lock(&g_proc_lock);
    for (i = 0; i < g_process_count; i++) {
        if (g_processes[i].running)
            cb(g_processes[i].process_id, g_processes[i].image_path, ctx);
    }
    pthread_mutex_unlock(&g_proc_lock);
}

/* Attend la terminaison d'un processus (stub: retourne immédiatement). */
DWORD ProcessWaitForExit(HANDLE h, DWORD timeout_ms)
{
    (void)timeout_ms;
    (void)h;
    return WAIT_OBJECT_0;
}
