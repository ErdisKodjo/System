#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "../include/runtime_manager.h"

/**
 * @file android_runtime_manager.cpp
 * @brief Android runtime manager: invokes the AfriOS AndroSandbox
 *        (dalvikvm + binder service_manager + surface_flinger) to host
 *        APK / DEX binaries.
 *
 * Public API (extern "C"):
 *   - AndroidRuntimeInit()        : start service_manager + surface_flinger
 *   - AndroidRuntimeSpawnApk()    : launch an APK via dalvikvm
 *   - AndroidRuntimeShutdown()    : tear down all sandboxes
 *
 * C++ is used because the Android framework expects C++ interop with the
 * binder NDK and ART runtime; all public functions are extern "C" so they
 * can be called from the C orchestrator.
 */

#define MAX_ANDROID_APPS 32
#define DALVIKVM_PATH    "../afros-androsandbox/dalvikvm"
#define BINDER_PATH      "../afros-androsandbox/binder"
#define SURFACE_FLINGER  "../afros-androsandbox/surface_flinger"

extern "C" {

struct android_app {
    pid_t    pid;
    char     package[128];
    uint32_t status;
    int      in_use;
};

static struct android_app g_apps[MAX_ANDROID_APPS];
static pid_t g_service_manager_pid = -1;
static pid_t g_surface_flinger_pid = -1;
static int   g_initialized = 0;

static struct android_app *slot_alloc(void)
{
    for (int i = 0; i < MAX_ANDROID_APPS; i++)
        if (!g_apps[i].in_use) {
            g_apps[i].in_use = 1;
            return &g_apps[i];
        }
    return nullptr;
}

static struct android_app *slot_find(pid_t pid)
{
    for (int i = 0; i < MAX_ANDROID_APPS; i++)
        if (g_apps[i].in_use && g_apps[i].pid == pid)
            return &g_apps[i];
    return nullptr;
}

afros_status_t AndroidRuntimeInit(void)
{
    if (g_initialized)
        return AFROS_SUCCESS;
    std::memset(g_apps, 0, sizeof(g_apps));

    /* Start the binder service_manager. */
    g_service_manager_pid = fork();
    if (g_service_manager_pid == 0) {
        char *const argv[] = { (char *)BINDER_PATH,
                              (char *)"service_manager", nullptr };
        execv(BINDER_PATH, argv);
        _exit(127);
    }

    /* Start surface_flinger (the display compositor). */
    g_surface_flinger_pid = fork();
    if (g_surface_flinger_pid == 0) {
        char *const argv[] = { (char *)SURFACE_FLINGER, nullptr };
        execv(SURFACE_FLINGER, argv);
        _exit(127);
    }

    /* Give the daemons a moment to register their binder services. */
    usleep(100 * 1000);
    g_initialized = 1;
    return AFROS_SUCCESS;
}

afros_status_t AndroidRuntimeSpawnApk(const char *apk_path,
                                      const char *package,
                                      const char *activity,
                                      pid_t *out_pid)
{
    struct android_app *slot;
    pid_t pid;

    if (!g_initialized) AndroidRuntimeInit();
    if (!apk_path || !package) return AFROS_ERROR_INVALID_PARAM;
    slot = slot_alloc();
    if (!slot) return AFROS_ERROR_NO_MEMORY;

    pid = fork();
    if (pid == 0) {
        /* Child: invoke dalvikvm with the APK + main activity. */
        char cmdline[2048];
        if (activity) {
            std::snprintf(cmdline, sizeof(cmdline),
                          "%s -classpath %s android.app.ActivityThread --pkg=%s --act=%s",
                          DALVIKVM_PATH, apk_path, package, activity);
        } else {
            std::snprintf(cmdline, sizeof(cmdline),
                          "%s -classpath %s android.app.ActivityThread --pkg=%s",
                          DALVIKVM_PATH, apk_path, package);
        }
        char *const argv[] = { (char *)"/bin/sh", (char *)"-c",
                               cmdline, nullptr };
        execv("/bin/sh", argv);
        _exit(127);
    }
    if (pid < 0) {
        slot->in_use = 0;
        return AFROS_ERROR;
    }
    slot->pid    = pid;
    slot->status = 0;
    std::strncpy(slot->package, package, sizeof(slot->package) - 1);
    slot->package[sizeof(slot->package) - 1] = '\0';
    if (out_pid) *out_pid = pid;
    return AFROS_SUCCESS;
}

afros_status_t AndroidRuntimeSignal(pid_t pid, int signo)
{
    struct android_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (kill(pid, signo) != 0)
        return AFROS_ERROR;
    return AFROS_SUCCESS;
}

afros_status_t AndroidRuntimeWait(pid_t pid, int *exit_code)
{
    int status = 0;
    struct android_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (waitpid(pid, &status, 0) < 0)
        return AFROS_ERROR;
    if (WIFEXITED(status))        s->status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->status = 0x8000 | WTERMSIG(status);
    if (exit_code) *exit_code = (int)s->status;
    s->in_use = 0;
    return AFROS_SUCCESS;
}

afros_status_t AndroidRuntimeShutdown(void)
{
    for (int i = 0; i < MAX_ANDROID_APPS; i++) {
        if (g_apps[i].in_use && g_apps[i].pid > 0)
            kill(g_apps[i].pid, SIGKILL);
        g_apps[i].in_use = 0;
    }
    if (g_surface_flinger_pid > 0) {
        kill(g_surface_flinger_pid, SIGTERM);
        waitpid(g_surface_flinger_pid, nullptr, 0);
        g_surface_flinger_pid = -1;
    }
    if (g_service_manager_pid > 0) {
        kill(g_service_manager_pid, SIGTERM);
        waitpid(g_service_manager_pid, nullptr, 0);
        g_service_manager_pid = -1;
    }
    g_initialized = 0;
    return AFROS_SUCCESS;
}

/* ----- runtime_ops_t adapter ----- */

static afros_status_t android_initialize(void) { return AndroidRuntimeInit(); }

static afros_status_t android_load_app(const char *path)
{
    pid_t pid;
    /* Best-effort: treat path's basename as the package name. */
    const char *base = std::strrchr(path, '/');
    base = base ? base + 1 : path;
    return AndroidRuntimeSpawnApk(path, base, nullptr, &pid);
}

static afros_status_t android_start_app(const char *name)
{
    (void)name;
    return AFROS_SUCCESS;
}

static afros_status_t android_stop_app(const char *name)
{
    for (int i = 0; i < MAX_ANDROID_APPS; i++)
        if (g_apps[i].in_use && std::strcmp(g_apps[i].package, name) == 0)
            return AndroidRuntimeSignal(g_apps[i].pid, SIGTERM);
    return AFROS_ERROR_INVALID_PARAM;
}

static afros_status_t android_get_status(const char *name, uint32_t *status)
{
    for (int i = 0; i < MAX_ANDROID_APPS; i++)
        if (g_apps[i].in_use && std::strcmp(g_apps[i].package, name) == 0) {
            if (status) *status = g_apps[i].status;
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR_INVALID_PARAM;
}

const runtime_ops_t *AndroidRuntimeOps(void)
{
    static const runtime_ops_t ops = {
        .initialize = android_initialize,
        .load_app   = android_load_app,
        .start_app  = android_start_app,
        .stop_app   = android_stop_app,
        .get_status = android_get_status,
    };
    return &ops;
}

} /* extern "C" */
