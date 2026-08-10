#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "../include/version_mgmt.h"

/**
 * @file downloader.c
 * @brief Downloads a runtime archive via HTTP(S), verifies SHA-256,
 *        streams to disk.
 *
 * We shell out to curl (preferred) or wget for the actual HTTP transfer.
 * SHA-256 verification uses sha256sum if available; if not, the caller
 * can pass expected_sha256 = NULL to skip verification (with a warning
 * logged to stderr).
 */

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static int file_exists(const char *p)
{
    struct stat st;
    return (stat(p, &st) == 0);
}

/* Run a shell command and return its exit status. */
static int run_cmd(const char *cmd)
{
    return system(cmd);
}

/* Read first 64 hex chars of sha256sum output, or empty on failure. */
static int sha256_of_file(const char *path, char out[65])
{
    char cmd[1024];
    FILE *p;
    char line[256];
    int ok = 0;

    snprintf(cmd, sizeof(cmd), "sha256sum \"%s\" 2>/dev/null", path);
    p = popen(cmd, "r");
    if (!p) return -1;
    if (fgets(line, sizeof(line), p)) {
        size_t i = 0;
        while (i < 64 && line[i] && isxdigit((unsigned char)line[i])) {
            out[i] = (char)tolower((unsigned char)line[i]);
            i++;
        }
        out[i] = '\0';
        ok = (i == 64);
    }
    pclose(p);
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t DownloaderFetch(const char *url,
                               const char *dest_path,
                               const char *expected_sha256)
{
    char cmd[2048];
    int rc;

    if (!url || !dest_path)
        return AFROS_ERROR_INVALID_PARAM;

    /* Make sure the destination directory exists. */
    {
        char *dir_copy = strdup(dest_path);
        char *slash;
        if (dir_copy) {
            slash = strrchr(dir_copy, '/');
            if (slash) {
                *slash = '\0';
                if (dir_copy[0]) {
                    char mkcmd[1024];
                    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p \"%s\" 2>/dev/null", dir_copy);
                    (void)run_cmd(mkcmd);
                }
            }
            free(dir_copy);
        }
    }

    /* Prefer curl; fall back to wget. */
    snprintf(cmd, sizeof(cmd),
             "curl -fSL --retry 3 -o \"%s\" \"%s\" 2>/dev/null || "
             "wget -q --tries=3 -O \"%s\" \"%s\" 2>/dev/null",
             dest_path, url, dest_path, url);
    rc = run_cmd(cmd);
    if (rc != 0 || !file_exists(dest_path))
        return AFROS_ERROR;

    /* Verify SHA-256 if requested. */
    if (expected_sha256 && expected_sha256[0]) {
        char actual[65];
        char expected_lower[65];
        size_t i;
        if (sha256_of_file(dest_path, actual) != 0) {
            fprintf(stderr,
                    "[afros-downloader] WARNING: could not compute SHA-256 "
                    "of %s; verification skipped.\n", dest_path);
            return AFROS_SUCCESS;
        }
        for (i = 0; i < 64 && expected_sha256[i]; i++)
            expected_lower[i] = (char)tolower((unsigned char)expected_sha256[i]);
        expected_lower[i] = '\0';
        if (strcmp(actual, expected_lower) != 0) {
            fprintf(stderr,
                    "[afros-downloader] SHA-256 mismatch for %s:\n"
                    "  expected: %s\n"
                    "  actual:   %s\n",
                    dest_path, expected_lower, actual);
            (void)unlink(dest_path);
            return AFROS_ERROR;
        }
    }
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Convenience: download to a temp path and return the path.          */
/* ------------------------------------------------------------------ */

afros_status_t DownloaderFetchTemp(const char *url,
                                   const char *expected_sha256,
                                   char *out_path, size_t out_cap)
{
    char tmpl[256];
    int fd;
    afros_status_t r;

    snprintf(tmpl, sizeof(tmpl), "/tmp/afros-dl-XXXXXX");
    fd = mkstemp(tmpl);
    if (fd < 0) return AFROS_ERROR;
    close(fd);
    /* mkstemp creates an empty file; remove it so curl/wget can write it. */
    (void)unlink(tmpl);

    r = DownloaderFetch(url, tmpl, expected_sha256);
    if (r != AFROS_SUCCESS) {
        (void)unlink(tmpl);
        return r;
    }
    if (out_path && out_cap) {
        strncpy(out_path, tmpl, out_cap - 1);
        out_path[out_cap - 1] = '\0';
    }
    return AFROS_SUCCESS;
}
