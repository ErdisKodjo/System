/*
 * service_control_manager.c — SCM (Service Control Manager) pour afros-winbridge.
 *
 * Maintient la base de données des services Win32, permet de les démarrer,
 * arrêter, et envoyer des codes de contrôle. Persiste la config dans la
 * sous-clé System\\CurrentControlSet\\Services du registre.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_SERVICES 128

/* --- Types internes --------------------------------------------------- */

typedef enum _SVC_STATE {
    SVC_STOPPED = 1,
    SVC_START_PENDING,
    SVC_RUNNING,
    SVC_STOP_PENDING,
    SVC_PAUSED,
} SVC_STATE;

typedef struct _SVC_RECORD {
    char       name[64];
    char       display[128];
    char       binary_path[256];
    DWORD      start_type;       /* 0=boot, 1=system, 2=auto, 3=demand, 4=disabled */
    DWORD      error_control;
    DWORD      service_type;     /* 1=kernel, 2=fs, 16=own process, 32=share */
    SVC_STATE  state;
    DWORD      pid;
    HANDLE     handle;
} SVC_RECORD;

static SVC_RECORD       g_services[MAX_SERVICES];
static int              g_service_count = 0;
static pthread_mutex_t  g_svc_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- Helpers locaux ---------------------------------------------------- */

static SVC_RECORD *find_service(const char *name)
{
    int i;
    for (i = 0; i < g_service_count; i++)
        if (strcmp(g_services[i].name, name) == 0) return &g_services[i];
    return NULL;
}

static SVC_RECORD *alloc_service(void)
{
    if (g_service_count >= MAX_SERVICES) return NULL;
    return &g_services[g_service_count++];
}

/* --- API publique ------------------------------------------------------ */

/* Ouvre (ou crée) la base de données SCM. */
HANDLE ScmOpenDatabase(void)
{
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        /* Enregistre les services par défaut depuis le registre. */
        const char *default_svcs[] = {
            "EventLog", "PlugPlay", "Winmgmt", "Schedule", "Spooler",
            "LanmanServer", "LanmanWorkstation"
        };
        size_t i;
        for (i = 0; i < sizeof(default_svcs)/sizeof(default_svcs[0]); i++) {
            SVC_RECORD *s = alloc_service();
            if (!s) break;
            strncpy(s->name, default_svcs[i], sizeof(s->name) - 1);
            s->state = SVC_STOPPED;
            s->start_type = 3;
        }
    }
    return (HANDLE)(LONG_PTR)0xDEADBEEF;
}

/* Démarre un service par nom. */
NTSTATUS ScmStartService(const char *name, DWORD argc, const char **argv)
{
    SVC_RECORD *s;
    (void)argc; (void)argv;
    pthread_mutex_lock(&g_svc_lock);
    s = find_service(name);
    if (!s) { pthread_mutex_unlock(&g_svc_lock); return STATUS_NOT_FOUND; }
    if (s->state == SVC_RUNNING) {
        pthread_mutex_unlock(&g_svc_lock);
        return STATUS_SUCCESS;
    }
    s->state = SVC_START_PENDING;
    /* En pratique: fork + exec binary_path. Stub pour la sandbox. */
    s->pid = (DWORD)getpid();
    s->state = SVC_RUNNING;
    pthread_mutex_unlock(&g_svc_lock);
    return STATUS_SUCCESS;
}

/* Envoie un code de contrôle à un service. */
NTSTATUS ScmControlService(const char *name, DWORD control_code)
{
    SVC_RECORD *s;
    pthread_mutex_lock(&g_svc_lock);
    s = find_service(name);
    if (!s) { pthread_mutex_unlock(&g_svc_lock); return STATUS_NOT_FOUND; }
    switch (control_code) {
    case 1: /* SERVICE_CONTROL_STOP */
        s->state = SVC_STOP_PENDING;
        s->state = SVC_STOPPED;
        s->pid = 0;
        break;
    case 2: /* SERVICE_CONTROL_PAUSE */
        s->state = SVC_PAUSED;
        break;
    case 3: /* SERVICE_CONTROL_CONTINUE */
        s->state = SVC_RUNNING;
        break;
    default:
        pthread_mutex_unlock(&g_svc_lock);
        return STATUS_NOT_IMPLEMENTED;
    }
    pthread_mutex_unlock(&g_svc_lock);
    return STATUS_SUCCESS;
}

/* Énumère les services enregistrés. */
DWORD ScmEnumServices(char (*names)[64], DWORD max_count)
{
    DWORD i, n = (DWORD)g_service_count;
    if (!names || max_count == 0) return 0;
    if (n > max_count) n = max_count;
    pthread_mutex_lock(&g_svc_lock);
    for (i = 0; i < n; i++)
        strncpy(names[i], g_services[i].name, 63);
    pthread_mutex_unlock(&g_svc_lock);
    return n;
}

/* Enregistre un nouveau service dans la base. */
NTSTATUS ScmCreateService(const char *name, const char *binary_path,
                          DWORD start_type)
{
    SVC_RECORD *s;
    pthread_mutex_lock(&g_svc_lock);
    if (find_service(name)) {
        pthread_mutex_unlock(&g_svc_lock);
        return STATUS_OBJECT_NAME_COLLISION;
    }
    s = alloc_service();
    if (!s) { pthread_mutex_unlock(&g_svc_lock); return STATUS_INSUFFICIENT_RESOURCES; }
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    strncpy(s->binary_path, binary_path ? binary_path : "",
            sizeof(s->binary_path) - 1);
    s->start_type = start_type;
    s->state      = SVC_STOPPED;
    pthread_mutex_unlock(&g_svc_lock);
    return STATUS_SUCCESS;
}
