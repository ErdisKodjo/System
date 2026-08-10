/**
 * @file ios_sandbox.c
 * @brief iOS sandbox: per-app container directories and entitlement
 *        enforcement.
 *
 * On iOS every application runs inside a sandbox rooted at
 * ~/Library/Containers/<bundle-id>/. The sandbox enforces
 * entitlements declared in the app's signature.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

#define AFROS_SANDBOX_MAX_APPS 32

typedef struct {
    char    bundle_id[256];
    char    container[1024];
    bool    active;
} sandbox_app_t;

static sandbox_app_t g_apps[AFROS_SANDBOX_MAX_APPS];
static char g_current_bundle_id[256];
static bool g_sandbox_inited = false;

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

afros_status_t SandboxInit(const char *bundle_id) {
    if (!bundle_id) return AFROS_ERROR_INVALID_PARAM;
    if (!g_sandbox_inited) {
        memset(g_apps, 0, sizeof g_apps);
        g_sandbox_inited = true;
    }
    /* Look for an existing entry.                                    */
    for (int i = 0; i < AFROS_SANDBOX_MAX_APPS; i++) {
        if (g_apps[i].active && strcmp(g_apps[i].bundle_id, bundle_id) == 0) {
            strncpy(g_current_bundle_id, bundle_id,
                    sizeof g_current_bundle_id - 1);
            return AFROS_SUCCESS;
        }
    }
    /* Allocate a new entry.                                          */
    for (int i = 0; i < AFROS_SANDBOX_MAX_APPS; i++) {
        if (!g_apps[i].active) {
            strncpy(g_apps[i].bundle_id, bundle_id,
                    sizeof g_apps[i].bundle_id - 1);
            const char *home = getenv("HOME");
            if (!home) home = "/tmp";
            snprintf(g_apps[i].container, sizeof g_apps[i].container,
                     "%s/Library/Containers/%s", home, bundle_id);
            /* Create the standard subdirs.                           */
            char path[1280];
            const char *subdirs[] = {
                "Data", "Data/Documents", "Data/Library",
                "Data/Library/Preferences", "Data/Library/Caches",
                "Data/tmp", NULL
            };
            for (int j = 0; subdirs[j]; j++) {
                snprintf(path, sizeof path, "%s/%s",
                         g_apps[i].container, subdirs[j]);
                mkdir(path, 0700);
            }
            g_apps[i].active = true;
            strncpy(g_current_bundle_id, bundle_id,
                    sizeof g_current_bundle_id - 1);
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR_NO_MEMORY;
}

const char *SandboxGetContainer(const char *bundle_id) {
    const char *id = bundle_id ? bundle_id : g_current_bundle_id;
    if (id[0] == '\0') return NULL;
    for (int i = 0; i < AFROS_SANDBOX_MAX_APPS; i++) {
        if (g_apps[i].active && strcmp(g_apps[i].bundle_id, id) == 0) {
            return g_apps[i].container;
        }
    }
    return NULL;
}

const char *SandboxGetDocumentsDir(void) {
    static char doc[1024];
    const char *c = SandboxGetContainer(NULL);
    if (!c) return NULL;
    snprintf(doc, sizeof doc, "%s/Data/Documents", c);
    return doc;
}

const char *SandboxGetTmpDir(void) {
    static char tmp[1024];
    const char *c = SandboxGetContainer(NULL);
    if (!c) return NULL;
    snprintf(tmp, sizeof tmp, "%s/Data/tmp", c);
    return tmp;
}

const char *SandboxGetLibraryDir(void) {
    static char lib[1024];
    const char *c = SandboxGetContainer(NULL);
    if (!c) return NULL;
    snprintf(lib, sizeof lib, "%s/Data/Library", c);
    return lib;
}

/* ------------------------------------------------------------------ */
/* Entitlement enforcement                                             */
/* ------------------------------------------------------------------ */

afros_status_t SandboxCheckEntitlement(const char *key) {
    if (!key) return AFROS_ERROR_INVALID_PARAM;
    /* Delegate to the entitlements module.                          */
    return EntitlementsHas(key) ? AFROS_SUCCESS : AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Path access checks                                                  */
/* ------------------------------------------------------------------ */

afros_status_t SandboxCanAccessPath(const char *path, bool write) {
    (void)write;
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    const char *container = SandboxGetContainer(NULL);
    if (!container) return AFROS_SUCCESS; /* unsandboxed */
    if (strncmp(path, container, strlen(container)) != 0) {
        /* Outside the container — only allowed if entitlement present. */
        if (SandboxCheckEntitlement("com.apple.security.temporary-exception.files.absolute-path.read-write")
            == AFROS_SUCCESS) {
            return AFROS_SUCCESS;
        }
        return AFROS_ERROR;
    }
    return AFROS_SUCCESS;
}

afros_status_t SandboxEnumerateApps(void (*cb)(const char *bundle_id,
                                               const char *container,
                                               void *ctx), void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_SANDBOX_MAX_APPS; i++) {
        if (g_apps[i].active) {
            cb(g_apps[i].bundle_id, g_apps[i].container, ctx);
        }
    }
    return AFROS_SUCCESS;
}

afros_status_t SandboxReset(const char *bundle_id) {
    if (!bundle_id) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_SANDBOX_MAX_APPS; i++) {
        if (g_apps[i].active && strcmp(g_apps[i].bundle_id, bundle_id) == 0) {
            /* ContainerDestroy is implemented in container_manager.c */
            ContainerDestroy(bundle_id);
            g_apps[i].active = false;
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR;
}
