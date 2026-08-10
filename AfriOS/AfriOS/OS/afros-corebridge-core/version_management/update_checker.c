#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/version_mgmt.h"

/**
 * @file update_checker.c
 * @brief Polls a remote manifest URL for newer runtime versions.
 *
 * The manifest URL is https://afros.io/runtimes/manifest.json and has
 * the same shape as the local registry:
 *   { "runtimes": [ {"type":1,"version":"9.0","install_path":"","is_default":1}, ... ] }
 *
 * We shell out to curl (or wget) to fetch the manifest, parse it with
 * the same minimal hand-rolled parser used by version_registry.c, and
 * compare each remote entry to the locally-installed default for that
 * runtime type. If the remote version is strictly newer (by string
 * comparison, semver-ish) we report it as an update.
 */

#define MANIFEST_URL_DEFAULT "https://afros.io/runtimes/manifest.json"

static const char *manifest_url(void)
{
    const char *u = getenv("AFROS_MANIFEST_URL");
    return u ? u : MANIFEST_URL_DEFAULT;
}

/* ------------------------------------------------------------------ */
/* Tiny JSON parser (shared pattern with version_registry.c)          */
/* ------------------------------------------------------------------ */

static const char *find_field(const char *obj, const char *key)
{
    char pat[64];
    const char *p;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(obj, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n'))
        p++;
    return p;
}

static int parse_string(const char *p, char *out, size_t cap)
{
    size_t n = 0;
    if (!p || *p != '"') return -1;
    p++;
    while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
    out[n] = '\0';
    return (*p == '"') ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Version comparison: returns 1 if a > b (semver-ish), 0 otherwise.  */
/* ------------------------------------------------------------------ */

static int version_gt(const char *a, const char *b)
{
    long va[3] = {0,0,0}, vb[3] = {0,0,0};
    int na, nb, i;
    na = sscanf(a, "%ld.%ld.%ld", &va[0], &va[1], &va[2]);
    nb = sscanf(b, "%ld.%ld.%ld", &vb[0], &vb[1], &vb[2]);
    if (na <= 0 || nb <= 0) return strcmp(a, b) > 0;
    for (i = 0; i < 3 && i < na && i < nb; i++) {
        if (va[i] > vb[i]) return 1;
        if (va[i] < vb[i]) return 0;
    }
    return na > nb;
}

/* ------------------------------------------------------------------ */
/* Fetch the manifest into a heap buffer                              */
/* ------------------------------------------------------------------ */

static char *fetch_manifest(size_t *out_len)
{
    char cmd[1024];
    FILE *pipe;
    char *buf;
    size_t cap = 65536, len = 0;
    size_t n;

    snprintf(cmd, sizeof(cmd),
             "curl -fsSL \"%s\" 2>/dev/null || wget -qO- \"%s\" 2>/dev/null",
             manifest_url(), manifest_url());
    pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    buf = (char *)malloc(cap);
    if (!buf) { pclose(pipe); return NULL; }
    while ((n = fread(buf + len, 1, cap - len, pipe)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); pclose(pipe); return NULL; }
            buf = nb;
        }
    }
    pclose(pipe);
    if (len == 0) { free(buf); return NULL; }
    *out_len = len;
    return buf;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t UpdateCheck(runtime_type_t rt, version_t *latest)
{
    char  *manifest;
    size_t len;
    const char *p;
    char best_version[MAX_VERSION_STR] = "";
    char best_path[MAX_INSTALL_PATH]   = "";

    manifest = fetch_manifest(&len);
    if (!manifest)
        return AFROS_ERROR;

    p = manifest;
    while ((p = strstr(p, "\"type\"")) != NULL) {
        const char *obj = p;
        const char *obj_end = strchr(obj, '}');
        const char *q;
        long t = 0;
        char vstr[MAX_VERSION_STR] = "";
        char ipath[MAX_INSTALL_PATH] = "";

        if (!obj_end) break;
        q = find_field(obj, "type");
        if (q) t = strtol(q, NULL, 10);
        if ((runtime_type_t)t != rt) { p = obj_end + 1; continue; }
        q = find_field(obj, "version");
        if (q) parse_string(q, vstr, sizeof(vstr));
        q = find_field(obj, "install_path");
        if (q) parse_string(q, ipath, sizeof(ipath));

        if (version_gt(vstr, best_version)) {
            strncpy(best_version, vstr, sizeof(best_version) - 1);
            best_version[sizeof(best_version) - 1] = '\0';
            strncpy(best_path, ipath, sizeof(best_path) - 1);
            best_path[sizeof(best_path) - 1] = '\0';
        }
        p = obj_end + 1;
    }
    free(manifest);

    if (best_version[0] == '\0')
        return AFROS_ERROR;

    if (latest) {
        memset(latest, 0, sizeof(*latest));
        latest->type = rt;
        strncpy(latest->version, best_version, sizeof(latest->version) - 1);
        strncpy(latest->install_path, best_path, sizeof(latest->install_path) - 1);
        latest->is_default = 0;
    }
    return AFROS_SUCCESS;
}

uint32_t UpdateCheckAll(version_t *updates, uint32_t max)
{
    static const runtime_type_t types[] = {
        RUNTIME_TYPE_NATIVE,
        RUNTIME_TYPE_LINUX,
        RUNTIME_TYPE_WINBRIDGE,
        RUNTIME_TYPE_ANDROID,
        RUNTIME_TYPE_IOS,
        RUNTIME_TYPE_HARMONY,
    };
    uint32_t n = 0;
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        version_t local, remote;
        if (VersionGetDefault(types[i], &local) != AFROS_SUCCESS)
            memset(&local, 0, sizeof(local));
        if (UpdateCheck(types[i], &remote) != AFROS_SUCCESS)
            continue;
        if (version_gt(remote.version, local.version)) {
            if (updates && n < max)
                updates[n] = remote;
            n++;
        }
    }
    return n;
}
