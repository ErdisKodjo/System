#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "../include/runtime_manager.h"

/**
 * @file win_runtime_manager.c
 * @brief Windows runtime manager: invokes the AfriOS WinBridge
 *        (afros-winbridge/wine/server/wineserver +
 *         afros-winbridge/wine/loader/wine_loader.c) to host PE binaries.
 *
 * Public API:
 *   - WinRuntimeInit()        : start a per-user wineserver, set WINEPREFIX
 *   - WinRuntimeSpawn()       : launch a PE binary via wine loader
 *   - WinRuntimeShutdown()    : gracefully stop wineserver, free state
 */

#define MAX_WIN_APPS  32
#define WINEPREFIX_DEFAULT "/var/lib/afros/wineprefix"
#define WINESERVER_PATH    "../afros-winbridge/wine/server/wineserver"
#define WINELOADER_PATH    "../afros-winbridge/wine/loader/wine_loader"

struct win_app {
    pid_t    pid;
    char     name[64];
    uint32_t status;
    int      in_use;
};

static struct win_app g_apps[MAX_WIN_APPS];
static pid_t          g_wineserver_pid = -1;
static int            g_initialized    = 0;
static char           g_wineprefix[512];

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static struct win_app *slot_alloc(void)
{
    for (int i = 0; i < MAX_WIN_APPS; i++)
        if (!g_apps[i].in_use) {
            g_apps[i].in_use = 1;
            return &g_apps[i];
        }
    return NULL;
}

static struct win_app *slot_find(pid_t pid)
{
    for (int i = 0; i < MAX_WIN_APPS; i++)
        if (g_apps[i].in_use && g_apps[i].pid == pid)
            return &g_apps[i];
    return NULL;
}

static void set_env(const char *k, const char *v)
{
    setenv(k, v, 1);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t WinRuntimeInit(void)
{
    const char *prefix;

    if (g_initialized)
        return AFROS_SUCCESS;
    memset(g_apps, 0, sizeof(g_apps));

    prefix = getenv("WINEPREFIX");
    if (!prefix || !*prefix)
        prefix = WINEPREFIX_DEFAULT;
    strncpy(g_wineprefix, prefix, sizeof(g_wineprefix) - 1);
    g_wineprefix[sizeof(g_wineprefix) - 1] = '\0';
    set_env("WINEPREFIX", g_wineprefix);

    /* Best-effort: ensure the prefix directory exists. */
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" 2>/dev/null", g_wineprefix);
        (void)system(cmd);
    }

    /* Launch the wineserver in the background. If it isn't present on
     * the host, we still mark ourselves initialized so that subsequent
     * spawn calls fail gracefully rather than crash the orchestrator. */
    g_wineserver_pid = fork();
    if (g_wineserver_pid == 0) {
        /* Child */
        char *const argv[] = { (char *)WINESERVER_PATH, "-p", NULL };
        execv(WINESERVER_PATH, argv);
        _exit(127);
    }
    g_initialized = 1;
    return AFROS_SUCCESS;
}

afros_status_t WinRuntimeSpawn(const char *path, const char *args,
                               pid_t *out_pid)
{
    struct win_app *slot;
    pid_t pid;
    char arg_copy[1024];
    char *argv[16];
    int   argc = 0;

    if (!g_initialized) WinRuntimeInit();
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    slot = slot_alloc();
    if (!slot) return AFROS_ERROR_NO_MEMORY;

    argv[argc++] = (char *)WINELOADER_PATH;
    argv[argc++] = (char *)path;
    if (args) {
        strncpy(arg_copy, args, sizeof(arg_copy) - 1);
        arg_copy[sizeof(arg_copy) - 1] = '\0';
        char *tok = strtok(arg_copy, " \t");
        while (tok && argc < 15) {
            argv[argc++] = tok;
            tok = strtok(NULL, " \t");
        }
    }
    argv[argc] = NULL;

    pid = fork();
    if (pid == 0) {
        execv(WINELOADER_PATH, argv);
        _exit(127);
    }
    if (pid < 0) {
        slot->in_use = 0;
        return AFROS_ERROR;
    }
    slot->pid    = pid;
    slot->status = 0;
    strncpy(slot->name, path, sizeof(slot->name) - 1);
    slot->name[sizeof(slot->name) - 1] = '\0';
    if (out_pid) *out_pid = pid;
    return AFROS_SUCCESS;
}

afros_status_t WinRuntimeSignal(pid_t pid, int signo)
{
    struct win_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (kill(pid, signo) != 0)
        return AFROS_ERROR;
    return AFROS_SUCCESS;
}

afros_status_t WinRuntimeWait(pid_t pid, int *exit_code)
{
    int status = 0;
    struct win_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (waitpid(pid, &status, 0) < 0)
        return AFROS_ERROR;
    if (WIFEXITED(status))      s->status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->status = 0x8000 | WTERMSIG(status);
    if (exit_code) *exit_code = (int)s->status;
    s->in_use = 0;
    return AFROS_SUCCESS;
}

afros_status_t WinRuntimeShutdown(void)
{
    for (int i = 0; i < MAX_WIN_APPS; i++) {
        if (g_apps[i].in_use && g_apps[i].pid > 0)
            kill(g_apps[i].pid, SIGKILL);
        g_apps[i].in_use = 0;
    }
    if (g_wineserver_pid > 0) {
        kill(g_wineserver_pid, SIGTERM);
        waitpid(g_wineserver_pid, NULL, 0);
        g_wineserver_pid = -1;
    }
    g_initialized = 0;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* runtime_ops_t adapter                                              */
/* ------------------------------------------------------------------ */

static afros_status_t win_initialize(void) { return WinRuntimeInit(); }

static afros_status_t win_load_app(const char *path)
{
    pid_t pid;
    return WinRuntimeSpawn(path, NULL, &pid);
}

static afros_status_t win_start_app(const char *name)
{
    (void)name;
    return AFROS_SUCCESS;
}

static afros_status_t win_stop_app(const char *name)
{
    for (int i = 0; i < MAX_WIN_APPS; i++)
        if (g_apps[i].in_use && strcmp(g_apps[i].name, name) == 0)
            return WinRuntimeSignal(g_apps[i].pid, SIGTERM);
    return AFROS_ERROR_INVALID_PARAM;
}

static afros_status_t win_get_status(const char *name, uint32_t *status)
{
    for (int i = 0; i < MAX_WIN_APPS; i++)
        if (g_apps[i].in_use && strcmp(g_apps[i].name, name) == 0) {
            if (status) *status = g_apps[i].status;
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR_INVALID_PARAM;
}

const runtime_ops_t *WinRuntimeOps(void)
{
    static const runtime_ops_t ops = {
        .initialize = win_initialize,
        .load_app   = win_load_app,
        .start_app  = win_start_app,
        .stop_app   = win_stop_app,
        .get_status = win_get_status,
    };
    return &ops;
}
