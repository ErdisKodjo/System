#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "../include/runtime_manager.h"

/**
 * @file harmony_runtime_manager.c
 * @brief HarmonyOS runtime manager: invokes the AfriOS HarmonyGate
 *        (ability/ability_runtime) to host .hap / .hsp packages.
 *
 * Public API:
 *   - HarmonyRuntimeInit()        : start ability_runtime daemon
 *   - HarmonyRuntimeSpawnHap()    : launch a HAP via ability_runtime
 *   - HarmonyRuntimeShutdown()    : tear down all running abilities
 */

#define MAX_HARMONY_APPS 32
#define ABILITY_RUNTIME_PATH "../afros-harmonygate/ability/ability_runtime"

struct harmony_app {
    pid_t    pid;
    char     bundle[128];
    char     ability[128];
    uint32_t status;
    int      in_use;
};

static struct harmony_app g_apps[MAX_HARMONY_APPS];
static pid_t g_ability_runtime_pid = -1;
static int   g_initialized = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static struct harmony_app *slot_alloc(void)
{
    for (int i = 0; i < MAX_HARMONY_APPS; i++)
        if (!g_apps[i].in_use) {
            g_apps[i].in_use = 1;
            return &g_apps[i];
        }
    return NULL;
}

static struct harmony_app *slot_find(pid_t pid)
{
    for (int i = 0; i < MAX_HARMONY_APPS; i++)
        if (g_apps[i].in_use && g_apps[i].pid == pid)
            return &g_apps[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t HarmonyRuntimeInit(void)
{
    if (g_initialized)
        return AFROS_SUCCESS;
    memset(g_apps, 0, sizeof(g_apps));

    /* Start the ability_runtime daemon — it hosts the Ability framework
     * (AbilityManagerService, AppMgrService, AMS scheduler). */
    g_ability_runtime_pid = fork();
    if (g_ability_runtime_pid == 0) {
        char *const argv[] = { (char *)ABILITY_RUNTIME_PATH,
                               (char *)"--daemon", NULL };
        execv(ABILITY_RUNTIME_PATH, argv);
        _exit(127);
    }
    usleep(150 * 1000); /* let the daemon register its services */
    g_initialized = 1;
    return AFROS_SUCCESS;
}

afros_status_t HarmonyRuntimeSpawnHap(const char *hap_path,
                                      const char *bundle,
                                      const char *ability,
                                      pid_t *out_pid)
{
    struct harmony_app *slot;
    pid_t pid;

    if (!g_initialized) HarmonyRuntimeInit();
    if (!hap_path) return AFROS_ERROR_INVALID_PARAM;
    slot = slot_alloc();
    if (!slot) return AFROS_ERROR_NO_MEMORY;

    pid = fork();
    if (pid == 0) {
        char cmdline[2048];
        snprintf(cmdline, sizeof(cmdline),
                 "%s --install=%s --bundle=%s --ability=%s",
                 ABILITY_RUNTIME_PATH, hap_path,
                 bundle ? bundle : "",
                 ability ? ability : "MainAbility");
        char *const argv[] = { (char *)"/bin/sh", (char *)"-c",
                               cmdline, NULL };
        execv("/bin/sh", argv);
        _exit(127);
    }
    if (pid < 0) {
        slot->in_use = 0;
        return AFROS_ERROR;
    }
    slot->pid    = pid;
    slot->status = 0;
    if (bundle) {
        strncpy(slot->bundle, bundle, sizeof(slot->bundle) - 1);
        slot->bundle[sizeof(slot->bundle) - 1] = '\0';
    }
    if (ability) {
        strncpy(slot->ability, ability, sizeof(slot->ability) - 1);
        slot->ability[sizeof(slot->ability) - 1] = '\0';
    }
    if (out_pid) *out_pid = pid;
    return AFROS_SUCCESS;
}

afros_status_t HarmonyRuntimeSignal(pid_t pid, int signo)
{
    struct harmony_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (kill(pid, signo) != 0)
        return AFROS_ERROR;
    return AFROS_SUCCESS;
}

afros_status_t HarmonyRuntimeWait(pid_t pid, int *exit_code)
{
    int status = 0;
    struct harmony_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (waitpid(pid, &status, 0) < 0)
        return AFROS_ERROR;
    if (WIFEXITED(status))        s->status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->status = 0x8000 | WTERMSIG(status);
    if (exit_code) *exit_code = (int)s->status;
    s->in_use = 0;
    return AFROS_SUCCESS;
}

afros_status_t HarmonyRuntimeShutdown(void)
{
    for (int i = 0; i < MAX_HARMONY_APPS; i++) {
        if (g_apps[i].in_use && g_apps[i].pid > 0)
            kill(g_apps[i].pid, SIGKILL);
        g_apps[i].in_use = 0;
    }
    if (g_ability_runtime_pid > 0) {
        kill(g_ability_runtime_pid, SIGTERM);
        waitpid(g_ability_runtime_pid, NULL, 0);
        g_ability_runtime_pid = -1;
    }
    g_initialized = 0;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* runtime_ops_t adapter                                              */
/* ------------------------------------------------------------------ */

static afros_status_t harmony_initialize(void) { return HarmonyRuntimeInit(); }

static afros_status_t harmony_load_app(const char *path)
{
    pid_t pid;
    return HarmonyRuntimeSpawnHap(path, NULL, NULL, &pid);
}

static afros_status_t harmony_start_app(const char *name)
{
    (void)name;
    return AFROS_SUCCESS;
}

static afros_status_t harmony_stop_app(const char *name)
{
    for (int i = 0; i < MAX_HARMONY_APPS; i++)
        if (g_apps[i].in_use && strcmp(g_apps[i].bundle, name) == 0)
            return HarmonyRuntimeSignal(g_apps[i].pid, SIGTERM);
    return AFROS_ERROR_INVALID_PARAM;
}

static afros_status_t harmony_get_status(const char *name, uint32_t *status)
{
    for (int i = 0; i < MAX_HARMONY_APPS; i++)
        if (g_apps[i].in_use && strcmp(g_apps[i].bundle, name) == 0) {
            if (status) *status = g_apps[i].status;
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR_INVALID_PARAM;
}

const runtime_ops_t *HarmonyRuntimeOps(void)
{
    static const runtime_ops_t ops = {
        .initialize = harmony_initialize,
        .load_app   = harmony_load_app,
        .start_app  = harmony_start_app,
        .stop_app   = harmony_stop_app,
        .get_status = harmony_get_status,
    };
    return &ops;
}
