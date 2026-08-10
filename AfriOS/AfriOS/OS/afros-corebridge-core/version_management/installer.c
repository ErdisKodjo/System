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
 * @file installer.c
 * @brief Extracts a downloaded runtime archive, registers it via
 *        VersionRegister, and validates by running the runtime's
 *        self-test. Supports rollback to the previous default version.
 *
 * Archive format: tar.gz or zip. Detection is done by extension.
 *
 * Installation layout:
 *   /opt/afros/<runtime_name>/<version>/
 *     bin/        <- runtime binaries
 *     lib/        <- runtime libraries
 *     share/      <- runtime data
 *     self-test   <- a script that exits 0 on success
 */

#define INSTALL_ROOT "/opt/afros"
#define MAX_VERSIONS_HINT 32

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static const char *runtime_name(runtime_type_t t)
{
    switch (t) {
    case RUNTIME_TYPE_NATIVE:    return "native";
    case RUNTIME_TYPE_LINUX:     return "linux";
    case RUNTIME_TYPE_WINBRIDGE: return "wine";
    case RUNTIME_TYPE_ANDROID:   return "android";
    case RUNTIME_TYPE_IOS:       return "darwin";
    case RUNTIME_TYPE_HARMONY:   return "harmony";
    default: return "unknown";
    }
}

static int has_suffix(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

static int ensure_dir(const char *path)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" 2>/dev/null", path);
    return system(cmd);
}

/* ------------------------------------------------------------------ */
/* Archive extraction                                                 */
/* ------------------------------------------------------------------ */

static afros_status_t extract_archive(const char *archive,
                                      const char *dest_dir)
{
    char cmd[2048];
    int rc;
    ensure_dir(dest_dir);
    if (has_suffix(archive, ".tar.gz") || has_suffix(archive, ".tgz")) {
        snprintf(cmd, sizeof(cmd), "tar -xzf \"%s\" -C \"%s\" 2>&1", archive, dest_dir);
    } else if (has_suffix(archive, ".tar.xz")) {
        snprintf(cmd, sizeof(cmd), "tar -xJf \"%s\" -C \"%s\" 2>&1", archive, dest_dir);
    } else if (has_suffix(archive, ".tar")) {
        snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\" 2>&1", archive, dest_dir);
    } else if (has_suffix(archive, ".zip")) {
        snprintf(cmd, sizeof(cmd), "unzip -q -o \"%s\" -d \"%s\" 2>&1", archive, dest_dir);
    } else {
        return AFROS_ERROR_NOT_SUPPORTED;
    }
    rc = system(cmd);
    return (rc == 0) ? AFROS_SUCCESS : AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Manifest extraction                                                */
/* ------------------------------------------------------------------ */

/* Extract runtime_type and version from the archive's MANIFEST.json
 * (if present). Returns AFROS_SUCCESS if found. */
static afros_status_t read_manifest(const char *dir,
                                    runtime_type_t *type,
                                    char *version, size_t vcap)
{
    char path[1024];
    FILE *fp;
    char buf[8192];
    size_t n;
    const char *p;

    snprintf(path, sizeof(path), "%s/MANIFEST.json", dir);
    fp = fopen(path, "r");
    if (!fp) return AFROS_ERROR;
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n == 0) return AFROS_ERROR;
    buf[n] = '\0';

    p = strstr(buf, "\"runtime_type\"");
    if (p) {
        p = strchr(p, ':');
        if (p) { p++; *type = (runtime_type_t)strtol(p, NULL, 10); }
    }
    p = strstr(buf, "\"version\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                size_t i = 0;
                p++;
                while (*p && *p != '"' && i + 1 < vcap)
                    version[i++] = *p++;
                version[i] = '\0';
            }
        }
    }
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                          */
/* ------------------------------------------------------------------ */

static afros_status_t run_self_test(const char *dir)
{
    char path[1024];
    char cmd[1024];
    int rc;
    snprintf(path, sizeof(path), "%s/self-test", dir);
    if (access(path, X_OK) != 0)
        return AFROS_SUCCESS; /* No self-test script: skip. */
    snprintf(cmd, sizeof(cmd), "\"%s\" >/dev/null 2>&1", path);
    rc = system(cmd);
    return (rc == 0) ? AFROS_SUCCESS : AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t InstallerInstall(const char *archive_path)
{
    char dest_dir[1024];
    runtime_type_t type = RUNTIME_TYPE_NATIVE;
    char version[MAX_VERSION_STR] = "0.0";
    version_t v;
    afros_status_t rc;

    if (!archive_path) return AFROS_ERROR_INVALID_PARAM;

    /* Stage 1: extract to a temp dir. */
    snprintf(dest_dir, sizeof(dest_dir), "%s/.stagingXXXXXX", INSTALL_ROOT);
    /* mkdtemp requires a writable template ending in XXXXXX. */
    if (mkdtemp(dest_dir) == NULL) {
        /* Fall back to a fixed staging dir. */
        snprintf(dest_dir, sizeof(dest_dir), "%s/.staging", INSTALL_ROOT);
        ensure_dir(dest_dir);
    }
    ensure_dir(INSTALL_ROOT);
    rc = extract_archive(archive_path, dest_dir);
    if (rc != AFROS_SUCCESS) return rc;

    /* Stage 2: read MANIFEST.json if present. */
    (void)read_manifest(dest_dir, &type, version, sizeof(version));

    /* Stage 3: run self-test. */
    rc = run_self_test(dest_dir);
    if (rc != AFROS_SUCCESS) {
        char cmd[1024];
        fprintf(stderr, "[afros-installer] self-test failed for %s\n", archive_path);
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dest_dir);
        (void)system(cmd);
        return rc;
    }

    /* Stage 4: move staging dir to its final location. */
    {
        char final_dir[1024];
        char cmd[2048];
        snprintf(final_dir, sizeof(final_dir), "%s/%s/%s",
                 INSTALL_ROOT, runtime_name(type), version);
        ensure_dir(final_dir);
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mv \"%s\"/* \"%s\"/ 2>/dev/null",
                 final_dir, dest_dir, final_dir);
        (void)system(cmd);
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dest_dir);
        (void)system(cmd);
        memset(&v, 0, sizeof(v));
        v.type = type;
        strncpy(v.version, version, sizeof(v.version) - 1);
        strncpy(v.install_path, final_dir, sizeof(v.install_path) - 1);
        v.is_default = 0;
    }

    /* Stage 5: register. */
    rc = VersionRegister(&v);
    return rc;
}

afros_status_t InstallerRollback(runtime_type_t rt)
{
    version_t current, prev;
    version_t all[MAX_VERSIONS_HINT];
    uint32_t n, i;
    int found_prev = 0;

    if (VersionGetDefault(rt, &current) != AFROS_SUCCESS)
        return AFROS_ERROR;

    /* List all installed versions of this type; pick the one that's not
     * the current default (prefer the highest remaining version). */
    n = VersionList(all, sizeof(all)/sizeof(all[0]));
    memset(&prev, 0, sizeof(prev));
    for (i = 0; i < n; i++) {
        if (all[i].type != rt) continue;
        if (strcmp(all[i].version, current.version) == 0) continue;
        if (!found_prev || strcmp(all[i].version, prev.version) > 0) {
            prev = all[i];
            found_prev = 1;
        }
    }
    if (!found_prev)
        return AFROS_ERROR;
    return VersionSetDefault(rt, prev.version);
}
