#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "../include/version_mgmt.h"

/**
 * @file version_registry.c
 * @brief Registry of installed runtime versions.
 *
 * The registry is stored as JSON at /var/lib/afros/runtimes.json. We
 * deliberately use a hand-rolled parser/writer (no JSON library
 * dependency) to keep afros-corebridge-core self-contained.
 *
 * JSON format:
 *   {
 *     "runtimes": [
 *       {"type":1,"version":"6.1","install_path":"/opt/afros/wine/6.1","is_default":1},
 *       ...
 *     ]
 *   }
 */

#define REGISTRY_PATH "/var/lib/afros/runtimes.json"
#define MAX_VERSIONS  32

static version_t g_versions[MAX_VERSIONS];
static uint32_t  g_count = 0;
static int       g_loaded = 0;

/* ------------------------------------------------------------------ */
/* Path helpers                                                       */
/* ------------------------------------------------------------------ */

static const char *registry_path(void)
{
    const char *p = getenv("AFROS_RUNTIMES_JSON");
    return p ? p : REGISTRY_PATH;
}

/* ------------------------------------------------------------------ */
/* Minimal JSON read                                                  */
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
    if (*p != '"') return -1;
    p++;
    while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
    out[n] = '\0';
    return (*p == '"') ? 0 : -1;
}

static void load_registry(void)
{
    FILE *fp;
    char *buf;
    long  sz;
    const char *p;

    if (g_loaded) return;
    g_loaded = 1;
    g_count  = 0;

    fp = fopen(registry_path(), "r");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    if (sz <= 0 || sz > 1024 * 1024) { fclose(fp); return; }
    fseek(fp, 0, SEEK_SET);
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return; }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf); fclose(fp); return;
    }
    buf[sz] = '\0';
    fclose(fp);

    p = buf;
    while ((p = strstr(p, "\"type\"")) != NULL && g_count < MAX_VERSIONS) {
        version_t *v = &g_versions[g_count];
        const char *q;
        long t = 0;
        const char *obj = p;
        /* Find next } to bound the object. */
        const char *obj_end = strchr(obj, '}');
        if (!obj_end) break;
        q = find_field(obj, "type");
        if (q) t = strtol(q, NULL, 10);
        v->type = (runtime_type_t)t;
        q = find_field(obj, "version");
        if (q) parse_string(q, v->version, sizeof(v->version));
        q = find_field(obj, "install_path");
        if (q) parse_string(q, v->install_path, sizeof(v->install_path));
        q = find_field(obj, "is_default");
        if (q) v->is_default = (int)strtol(q, NULL, 10);
        else v->is_default = 0;
        g_count++;
        p = obj_end + 1;
    }
    free(buf);
}

/* ------------------------------------------------------------------ */
/* Minimal JSON write                                                 */
/* ------------------------------------------------------------------ */

static void save_registry(void)
{
    FILE *fp;
    uint32_t i;
    /* Ensure directory exists. */
    (void)system("mkdir -p /var/lib/afros 2>/dev/null");
    fp = fopen(registry_path(), "w");
    if (!fp) return;
    fputs("{\n  \"runtimes\": [\n", fp);
    for (i = 0; i < g_count; i++) {
        fprintf(fp,
                "    {\"type\":%d,\"version\":\"%s\",\"install_path\":\"%s\",\"is_default\":%d}%s\n",
                (int)g_versions[i].type,
                g_versions[i].version,
                g_versions[i].install_path,
                g_versions[i].is_default,
                (i + 1 < g_count) ? "," : "");
    }
    fputs("  ]\n}\n", fp);
    fclose(fp);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t VersionRegister(const version_t *v)
{
    uint32_t i;
    if (!v) return AFROS_ERROR_INVALID_PARAM;
    load_registry();
    /* Update existing entry if (type, version) matches. */
    for (i = 0; i < g_count; i++)
        if (g_versions[i].type == v->type &&
            strcmp(g_versions[i].version, v->version) == 0) {
            g_versions[i] = *v;
            save_registry();
            return AFROS_SUCCESS;
        }
    if (g_count >= MAX_VERSIONS) return AFROS_ERROR_NO_MEMORY;
    g_versions[g_count++] = *v;
    save_registry();
    return AFROS_SUCCESS;
}

afros_status_t VersionUnregister(runtime_type_t type, const char *version)
{
    uint32_t i;
    if (!version) return AFROS_ERROR_INVALID_PARAM;
    load_registry();
    for (i = 0; i < g_count; i++)
        if (g_versions[i].type == type &&
            strcmp(g_versions[i].version, version) == 0) {
            /* Shift down. */
            memmove(&g_versions[i], &g_versions[i + 1],
                    (g_count - i - 1) * sizeof(version_t));
            g_count--;
            save_registry();
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR_INVALID_PARAM;
}

uint32_t VersionList(version_t *out, uint32_t max)
{
    uint32_t i, n = 0;
    load_registry();
    for (i = 0; i < g_count && n < max; i++)
        out[n++] = g_versions[i];
    return n;
}

afros_status_t VersionGetDefault(runtime_type_t type, version_t *out)
{
    uint32_t i;
    load_registry();
    /* First look for is_default=1 of the right type. */
    for (i = 0; i < g_count; i++)
        if (g_versions[i].type == type && g_versions[i].is_default) {
            if (out) *out = g_versions[i];
            return AFROS_SUCCESS;
        }
    /* Otherwise fall back to the first registered entry of that type. */
    for (i = 0; i < g_count; i++)
        if (g_versions[i].type == type) {
            if (out) *out = g_versions[i];
            return AFROS_SUCCESS;
        }
    return AFROS_ERROR;
}

afros_status_t VersionSetDefault(runtime_type_t type, const char *version)
{
    uint32_t i;
    int found = 0;
    if (!version) return AFROS_ERROR_INVALID_PARAM;
    load_registry();
    for (i = 0; i < g_count; i++) {
        if (g_versions[i].type != type) continue;
        g_versions[i].is_default =
            (strcmp(g_versions[i].version, version) == 0) ? 1 : 0;
        if (g_versions[i].is_default) found = 1;
    }
    if (!found) return AFROS_ERROR_INVALID_PARAM;
    save_registry();
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Op table                                                           */
/* ------------------------------------------------------------------ */

static const version_mgmt_ops_t g_ops = {
    .register_ver = VersionRegister,
    .unregister   = VersionUnregister,
    .list         = VersionList,
    .get_default  = VersionGetDefault,
    .set_default  = VersionSetDefault,
    .check        = UpdateCheck,
    .fetch        = DownloaderFetch,
    .install      = InstallerInstall,
    .rollback     = InstallerRollback,
};

const version_mgmt_ops_t *VersionMgmtGetOps(void)
{
    return &g_ops;
}
