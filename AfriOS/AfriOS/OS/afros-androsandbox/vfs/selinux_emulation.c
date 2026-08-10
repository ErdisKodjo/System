/*
 * vfs/selinux_emulation.c — SELinux context emulation.
 *
 * Android enforces mandatory access control via SELinux. Every process
 * has a security context (a string like "u:r:untrusted_app:s0"), every
 * file has a label, and the kernel authorises each access based on a
 * policy. The userspace API is small: getcon(), setcon(), setfilecon(),
 * getfilecon(), security_check_context(), and an access-vector cache
 * (avc) for caching decisions.
 *
 * In the sandbox we don't enforce anything; we just store the context
 * and label strings and answer every avc_has_perm() query with "allow".
 * The API is preserved so apps that check their own context (e.g. to
 * decide whether to enable a debug feature) still work.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SELINUX_CTX_MAX   128
#define SELINUX_AVC_MAX   256
#define SELINUX_LABEL_MAX 128

struct avc_entry {
    int   in_use;
    char  scon[SELINUX_CTX_MAX];   /* source context */
    char  tcon[SELINUX_CTX_MAX];   /* target context */
    char  tclass[32];              /* target class */
    uint32_t perms;                /* requested permission bits */
    int   allowed;                 /* decision: 1=allow, 0=deny */
};

static char g_proc_con[SELINUX_CTX_MAX] = "u:r:untrusted_app:s0";
static struct avc_entry g_avc[SELINUX_AVC_MAX];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int Getcon(char **context) {
    if (!context) return -EINVAL;
    pthread_mutex_lock(&g_lock);
    *context = strdup(g_proc_con);
    pthread_mutex_unlock(&g_lock);
    return *context ? 0 : -ENOMEM;
}

int Setcon(const char *context) {
    if (!context) return -EINVAL;
    pthread_mutex_lock(&g_lock);
    snprintf(g_proc_con, sizeof(g_proc_con), "%s", context);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int Getpidcon(int pid, char **context) {
    (void)pid;
    return Getcon(context);
}

/* Set the SELinux label on a file. Sandbox: store as an xattr-like side
 * file (we can't really label an arbitrary file from userspace). */
int Setfilecon(const char *path, const char *context) {
    char side[1024];
    FILE *f;
    if (!path || !context) return -EINVAL;
    snprintf(side, sizeof(side), "%s.seclabel", path);
    f = fopen(side, "w");
    if (!f) return -errno;
    fprintf(f, "%s\n", context);
    fclose(f);
    return 0;
}

int Getfilecon(const char *path, char **context) {
    char side[1024];
    char buf[SELINUX_CTX_MAX];
    FILE *f;
    if (!path || !context) return -EINVAL;
    *context = NULL;
    snprintf(side, sizeof(side), "%s.seclabel", path);
    f = fopen(side, "r");
    if (!f) {
        /* No label file: return a default. */
        *context = strdup("u:object_r:default_android_file:s0");
        return *context ? 0 : -ENOMEM;
    }
    if (fgets(buf, sizeof(buf), f) == NULL) buf[0] = 0;
    fclose(f);
    /* Trim trailing newline. */
    size_t n = strlen(buf);
    if (n && buf[n - 1] == '\n') buf[n - 1] = 0;
    *context = strdup(buf);
    return *context ? 0 : -ENOMEM;
}

int SecurityCheckContext(const char *context) {
    /* Sandbox: any non-NULL, non-empty context is "valid". */
    if (!context || !*context) return -EINVAL;
    return 0;
}

/* Access-vector cache lookup. Sandbox: always allow, but cache the
 * decision so repeated queries with the same args are O(1). */
int AvcHasPerm(const char *scon, const char *tcon, const char *tclass,
               uint32_t perms, void * /*auditdata*/) {
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < SELINUX_AVC_MAX; i++) {
        if (g_avc[i].in_use &&
            strcmp(g_avc[i].scon, scon ? scon : "") == 0 &&
            strcmp(g_avc[i].tcon, tcon ? tcon : "") == 0 &&
            strcmp(g_avc[i].tclass, tclass ? tclass : "") == 0 &&
            g_avc[i].perms == perms) {
            int a = g_avc[i].allowed;
            pthread_mutex_unlock(&g_lock);
            return a ? 0 : -EACCES;
        }
    }
    /* Insert a new entry. Sandbox policy: allow everything. */
    for (size_t i = 0; i < SELINUX_AVC_MAX; i++) {
        if (!g_avc[i].in_use) {
            g_avc[i].in_use = 1;
            snprintf(g_avc[i].scon, sizeof(g_avc[i].scon), "%s", scon ? scon : "");
            snprintf(g_avc[i].tcon, sizeof(g_avc[i].tcon), "%s", tcon ? tcon : "");
            snprintf(g_avc[i].tclass, sizeof(g_avc[i].tclass), "%s", tclass ? tclass : "");
            g_avc[i].perms = perms;
            g_avc[i].allowed = 1;
            pthread_mutex_unlock(&g_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return 0; /* cache full: still allow */
}

int AvcReset(void) {
    pthread_mutex_lock(&g_lock);
    memset(g_avc, 0, sizeof(g_avc));
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int AvcFlush(void) __attribute__((alias("AvcReset")));

/* Convenience: parse a context string into its 4 colon-separated parts
 * (user, role, type, level). Returns 0 on success. */
int SelinuxContextSplit(const char *context,
                        char *user, size_t user_max,
                        char *role, size_t role_max,
                        char *type, size_t type_max,
                        char *level, size_t level_max) {
    const char *p, *q;
    if (!context) return -EINVAL;
    p = context;
    q = strchr(p, ':');
    if (!q) return -EINVAL;
    if (user) snprintf(user, user_max, "%.*s", (int)(q - p), p);
    p = q + 1; q = strchr(p, ':');
    if (!q) return -EINVAL;
    if (role) snprintf(role, role_max, "%.*s", (int)(q - p), p);
    p = q + 1; q = strchr(p, ':');
    if (!q) return -EINVAL;
    if (type) snprintf(type, type_max, "%.*s", (int)(q - p), p);
    p = q + 1;
    if (level) snprintf(level, level_max, "%s", p);
    return 0;
}
