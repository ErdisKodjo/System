#define _GNU_SOURCE 1  /* Required for CLONE_NEWNS / CLONE_NEWPID / clone() */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "../include/runtime_manager.h"

/**
 * @file linux_runtime_manager.c
 * @brief Linux runtime manager: forks/execves a Linux process inside an
 *        AfriOS namespace (CLONE_NEWNS | CLONE_NEWPID) for sandboxing.
 *
 * Public API (in addition to runtime_ops_t):
 *   - LinuxRuntimeInit()        : one-time setup of /proc /sys bind mounts
 *   - LinuxRuntimeSpawn()       : clone() + execve() inside a new ns
 *   - LinuxRuntimeSignal(pid,t) : forward a signal to the sandboxed PID
 *   - LinuxRuntimeWait(pid)     : waitpid() the sandboxed PID
 *   - LinuxRuntimeShutdown()    : tear down all live sandboxes
 */

#define MAX_LINUX_APPS 32

struct linux_app {
    pid_t    pid;
    char     name[64];
    uint32_t status;
    int      in_use;
};

static struct linux_app g_apps[MAX_LINUX_APPS];
static int              g_initialized = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static struct linux_app *slot_alloc(void)
{
    for (int i = 0; i < MAX_LINUX_APPS; i++)
        if (!g_apps[i].in_use) {
            g_apps[i].in_use = 1;
            return &g_apps[i];
        }
    return NULL;
}

static struct linux_app *slot_find(pid_t pid)
{
    for (int i = 0; i < MAX_LINUX_APPS; i++)
        if (g_apps[i].in_use && g_apps[i].pid == pid)
            return &g_apps[i];
    return NULL;
}

/* Child entry point after clone(). Runs the target binary. */
static int linux_child(void *arg)
{
    char *const *argv = (char *const *)arg;
    /* Reset signal mask and groups. */
    setsid();
    execv(argv[0], argv);
    /* If execv fails, exit the child. */
    _exit(127);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t LinuxRuntimeInit(void)
{
    if (g_initialized)
        return AFROS_SUCCESS;
    memset(g_apps, 0, sizeof(g_apps));
    /* In a real deployment this would bind-mount /proc, /sys, /dev into
     * the runtime rootfs. On the host simulator we just mark initialized. */
    g_initialized = 1;
    return AFROS_SUCCESS;
}

afros_status_t LinuxRuntimeSpawn(const char *path, const char *args,
                                 pid_t *out_pid)
{
    struct linux_app *slot;
    char *argv_buf[16];
    char  arg_copy[1024];
    char *tok;
    int   argc = 0;
    int   flags;
    char *stack;
    pid_t pid;

    if (!g_initialized) LinuxRuntimeInit();
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    slot = slot_alloc();
    if (!slot) return AFROS_ERROR_NO_MEMORY;

    /* Build argv: [path, args..., NULL]. */
    argv_buf[argc++] = (char *)path;
    if (args) {
        strncpy(arg_copy, args, sizeof(arg_copy) - 1);
        arg_copy[sizeof(arg_copy) - 1] = '\0';
        tok = strtok(arg_copy, " \t");
        while (tok && argc < 15) {
            argv_buf[argc++] = tok;
            tok = strtok(NULL, " \t");
        }
    }
    argv_buf[argc] = NULL;

    /* Allocate a small stack for clone(). */
    stack = (char *)malloc(64 * 1024);
    if (!stack) {
        slot->in_use = 0;
        return AFROS_ERROR_NO_MEMORY;
    }

    /* New mount + PID namespaces. On systems where unprivileged
     * clone() with these flags is denied, fall back to fork(). */
    flags = CLONE_NEWNS | CLONE_NEWPID | SIGCHLD;
#ifdef CLONE_NEWUSER
    flags |= CLONE_NEWUSER;
#endif

    pid = clone(linux_child, stack + 64 * 1024, flags, argv_buf);
    if (pid < 0) {
        /* Fall back to plain fork. */
        pid = fork();
        if (pid == 0) {
            linux_child(argv_buf);
            _exit(127);
        }
    }
    free(stack);

    if (pid < 0) {
        slot->in_use = 0;
        return AFROS_ERROR;
    }
    slot->pid    = pid;
    slot->status = 0; /* running */
    strncpy(slot->name, path, sizeof(slot->name) - 1);
    slot->name[sizeof(slot->name) - 1] = '\0';
    if (out_pid) *out_pid = pid;
    return AFROS_SUCCESS;
}

afros_status_t LinuxRuntimeSignal(pid_t pid, int signo)
{
    struct linux_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (kill(pid, signo) != 0)
        return AFROS_ERROR;
    return AFROS_SUCCESS;
}

afros_status_t LinuxRuntimeWait(pid_t pid, int *exit_code)
{
    int status = 0;
    pid_t r;
    struct linux_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    r = waitpid(pid, &status, 0);
    if (r < 0) return AFROS_ERROR;
    if (WIFEXITED(status))      s->status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->status = 0x8000 | WTERMSIG(status);
    if (exit_code) *exit_code = (int)s->status;
    s->in_use = 0;
    return AFROS_SUCCESS;
}

afros_status_t LinuxRuntimeShutdown(void)
{
    for (int i = 0; i < MAX_LINUX_APPS; i++) {
        if (g_apps[i].in_use && g_apps[i].pid > 0)
            kill(g_apps[i].pid, SIGKILL);
        g_apps[i].in_use = 0;
    }
    g_initialized = 0;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* runtime_ops_t adapter                                              */
/* ------------------------------------------------------------------ */

static afros_status_t linux_initialize(void) { return LinuxRuntimeInit(); }

static afros_status_t linux_load_app(const char *path)
{
    pid_t pid;
    return LinuxRuntimeSpawn(path, NULL, &pid);
}

static afros_status_t linux_start_app(const char *name)
{
    (void)name;
    return AFROS_SUCCESS; /* Already started by spawn. */
}

static afros_status_t linux_stop_app(const char *name)
{
    for (int i = 0; i < MAX_LINUX_APPS; i++)
        if (g_apps[i].in_use && strcmp(g_apps[i].name, name) == 0)
            return LinuxRuntimeSignal(g_apps[i].pid, SIGTERM);
    return AFROS_ERROR_INVALID_PARAM;
}

static afros_status_t linux_get_status(const char *name, uint32_t *status)
{
    for (int i = 0; i < MAX_LINUX_APPS; i++)
        if (g_apps[i].in_use && strcmp(g_apps[i].name, name) == 0) {
            if (status) *status = g_apps[i].status;
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR_INVALID_PARAM;
}

const runtime_ops_t *LinuxRuntimeOps(void)
{
    static const runtime_ops_t ops = {
        .initialize = linux_initialize,
        .load_app   = linux_load_app,
        .start_app  = linux_start_app,
        .stop_app   = linux_stop_app,
        .get_status = linux_get_status,
    };
    return &ops;
}
