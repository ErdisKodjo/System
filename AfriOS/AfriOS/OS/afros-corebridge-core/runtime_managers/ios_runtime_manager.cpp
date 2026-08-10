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
 * @file ios_runtime_manager.cpp
 * @brief iOS / macOS runtime manager: invokes the AfriOS Incompat-Engine
 *        (darling + macho_loader) to host .app / Mach-O binaries.
 *
 * Public API (extern "C"):
 *   - IosRuntimeInit()        : set up dyld_emulator + objc_runtime
 *   - IosRuntimeSpawnApp()    : load a .app bundle
 *   - IosRuntimeShutdown()    : tear down all running apps
 *
 * C++ is used because the objc_runtime emulator needs C++ class interop
 * (Objective-C runtime metadata is parsed into C++ structs internally);
 * all public functions are extern "C" so they can be called from the C
 * orchestrator.
 */

#define MAX_IOS_APPS  32
#define DARLING_PATH   "../afros-incompat-engine/darling"
#define MACHO_LOADER   "../afros-incompat-engine/macho_loader"

extern "C" {

struct ios_app {
    pid_t    pid;
    char     bundle_id[128];
    uint32_t status;
    int      in_use;
};

static struct ios_app g_apps[MAX_IOS_APPS];
static int   g_initialized = 0;
static pid_t g_dyld_pid    = -1;
static pid_t g_objc_pid    = -1;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static struct ios_app *slot_alloc(void)
{
    for (int i = 0; i < MAX_IOS_APPS; i++)
        if (!g_apps[i].in_use) {
            g_apps[i].in_use = 1;
            return &g_apps[i];
        }
    return nullptr;
}

static struct ios_app *slot_find(pid_t pid)
{
    for (int i = 0; i < MAX_IOS_APPS; i++)
        if (g_apps[i].in_use && g_apps[i].pid == pid)
            return &g_apps[i];
    return nullptr;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t IosRuntimeInit(void)
{
    if (g_initialized)
        return AFROS_SUCCESS;
    std::memset(g_apps, 0, sizeof(g_apps));

    /* Start the dyld emulator (provides dyld stubs + Objective-C image
     * info registration). */
    g_dyld_pid = fork();
    if (g_dyld_pid == 0) {
        char *const argv[] = { (char *)DARLING_PATH,
                               (char *)"dyld_emulator", nullptr };
        execv(DARLING_PATH, argv);
        _exit(127);
    }

    /* Start the Objective-C runtime emulator (class lookup, method
     * dispatch, autorelease pools). */
    g_objc_pid = fork();
    if (g_objc_pid == 0) {
        char *const argv[] = { (char *)DARLING_PATH,
                               (char *)"objc_runtime", nullptr };
        execv(DARLING_PATH, argv);
        _exit(127);
    }
    usleep(100 * 1000);
    g_initialized = 1;
    return AFROS_SUCCESS;
}

afros_status_t IosRuntimeSpawnApp(const char *app_bundle,
                                  const char *bundle_id,
                                  pid_t *out_pid)
{
    struct ios_app *slot;
    pid_t pid;
    char exec_path[1024];

    if (!g_initialized) IosRuntimeInit();
    if (!app_bundle) return AFROS_ERROR_INVALID_PARAM;
    slot = slot_alloc();
    if (!slot) return AFROS_ERROR_NO_MEMORY;

    /* .app bundles conventionally have an executable with the same name
     * as the bundle (minus .app) under Contents/MacOS/. */
    {
        const char *slash = std::strrchr(app_bundle, '/');
        const char *base  = slash ? slash + 1 : app_bundle;
        char name[256];
        size_t n = std::strlen(base);
        if (n > 4 && std::strcmp(base + n - 4, ".app") == 0)
            n -= 4;
        if (n >= sizeof(name)) n = sizeof(name) - 1;
        std::memcpy(name, base, n);
        name[n] = '\0';
        std::snprintf(exec_path, sizeof(exec_path),
                      "%s/Contents/MacOS/%s", app_bundle, name);
    }

    pid = fork();
    if (pid == 0) {
        char *const argv[] = { (char *)MACHO_LOADER,
                               (char *)exec_path, nullptr };
        execv(MACHO_LOADER, argv);
        _exit(127);
    }
    if (pid < 0) {
        slot->in_use = 0;
        return AFROS_ERROR;
    }
    slot->pid    = pid;
    slot->status = 0;
    if (bundle_id) {
        std::strncpy(slot->bundle_id, bundle_id, sizeof(slot->bundle_id) - 1);
        slot->bundle_id[sizeof(slot->bundle_id) - 1] = '\0';
    } else {
        slot->bundle_id[0] = '\0';
    }
    if (out_pid) *out_pid = pid;
    return AFROS_SUCCESS;
}

afros_status_t IosRuntimeSignal(pid_t pid, int signo)
{
    struct ios_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (kill(pid, signo) != 0)
        return AFROS_ERROR;
    return AFROS_SUCCESS;
}

afros_status_t IosRuntimeWait(pid_t pid, int *exit_code)
{
    int status = 0;
    struct ios_app *s = slot_find(pid);
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (waitpid(pid, &status, 0) < 0)
        return AFROS_ERROR;
    if (WIFEXITED(status))        s->status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->status = 0x8000 | WTERMSIG(status);
    if (exit_code) *exit_code = (int)s->status;
    s->in_use = 0;
    return AFROS_SUCCESS;
}

afros_status_t IosRuntimeShutdown(void)
{
    for (int i = 0; i < MAX_IOS_APPS; i++) {
        if (g_apps[i].in_use && g_apps[i].pid > 0)
            kill(g_apps[i].pid, SIGKILL);
        g_apps[i].in_use = 0;
    }
    if (g_objc_pid > 0) {
        kill(g_objc_pid, SIGTERM);
        waitpid(g_objc_pid, nullptr, 0);
        g_objc_pid = -1;
    }
    if (g_dyld_pid > 0) {
        kill(g_dyld_pid, SIGTERM);
        waitpid(g_dyld_pid, nullptr, 0);
        g_dyld_pid = -1;
    }
    g_initialized = 0;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* runtime_ops_t adapter                                              */
/* ------------------------------------------------------------------ */

static afros_status_t ios_initialize(void) { return IosRuntimeInit(); }

static afros_status_t ios_load_app(const char *path)
{
    pid_t pid;
    return IosRuntimeSpawnApp(path, nullptr, &pid);
}

static afros_status_t ios_start_app(const char *name)
{
    (void)name;
    return AFROS_SUCCESS;
}

static afros_status_t ios_stop_app(const char *name)
{
    for (int i = 0; i < MAX_IOS_APPS; i++)
        if (g_apps[i].in_use && std::strcmp(g_apps[i].bundle_id, name) == 0)
            return IosRuntimeSignal(g_apps[i].pid, SIGTERM);
    return AFROS_ERROR_INVALID_PARAM;
}

static afros_status_t ios_get_status(const char *name, uint32_t *status)
{
    for (int i = 0; i < MAX_IOS_APPS; i++)
        if (g_apps[i].in_use && std::strcmp(g_apps[i].bundle_id, name) == 0) {
            if (status) *status = g_apps[i].status;
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR_INVALID_PARAM;
}

const runtime_ops_t *IosRuntimeOps(void)
{
    static const runtime_ops_t ops = {
        .initialize = ios_initialize,
        .load_app   = ios_load_app,
        .start_app  = ios_start_app,
        .stop_app   = ios_stop_app,
        .get_status = ios_get_status,
    };
    return &ops;
}

} /* extern "C" */
