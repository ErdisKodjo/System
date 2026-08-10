/**
 * @file provisioning_profile.c
 * @brief Parse iOS .mobileprovision files (CMS-wrapped plist).
 *
 * A mobileprovision file is a PKCS#7 CMS blob containing an XML
 * plist with the AppID, TeamID, certificate list and entitlements.
 * We extract only the parts used by the runtime.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* ------------------------------------------------------------------ */
/* Parsed profile                                                      */
/* ------------------------------------------------------------------ */

typedef struct provision_profile_s {
    char  app_id[256];
    char  team_id[64];
    char  creation_date[32];
    char  expiration_date[32];
    char *entitlements_xml;
    char *raw_plist;
    bool  valid;
} provision_profile_t;

static provision_profile_t g_profile = {0};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const char *find_plist_in_blob(const uint8_t *data, size_t size,
                                      size_t *out_len) {
    /* A .mobileprovision file starts with a binary CMS header        */
    /* followed by the text "<?xml ...". We scan for the XML prolog.   */
    const char *needle = "<?xml";
    const char *start  = NULL;
    for (size_t i = 0; i + 5 <= size; i++) {
        if (memcmp(data + i, needle, 5) == 0) {
            start = (const char *)(data + i);
            break;
        }
    }
    if (!start) return NULL;
    /* Find the closing </plist>.                                     */
    const char *end = strstr(start, "</plist>");
    if (!end) return NULL;
    *out_len = (size_t)(end - start) + strlen("</plist>");
    return start;
}

static char *extract_plist_value(const char *xml, size_t len,
                                 const char *key) {
    char pattern[256];
    snprintf(pattern, sizeof pattern, "<key>%s</key>", key);
    const char *p = strstr(xml, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    while (p < xml + len && (*p == ' ' || *p == '\n' || *p == '\t' ||
                             *p == '\r')) p++;
    const char *open = strstr(p, "<string>");
    if (!open) return NULL;
    open += strlen("<string>");
    const char *close = strstr(open, "</string>");
    if (!close) return NULL;
    size_t n = (size_t)(close - open);
    char *val = (char *)malloc(n + 1);
    if (!val) return NULL;
    memcpy(val, open, n);
    val[n] = '\0';
    return val;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t ProvisionLoad(const char *path) {
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return AFROS_ERROR;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return AFROS_ERROR;
    }
    void *map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return AFROS_ERROR;

    size_t plist_len = 0;
    const char *xml = find_plist_in_blob((const uint8_t *)map,
                                         (size_t)st.st_size, &plist_len);
    if (!xml) {
        munmap(map, (size_t)st.st_size);
        return AFROS_ERROR;
    }
    /* Free any previous profile contents.                            */
    free(g_profile.entitlements_xml);
    free(g_profile.raw_plist);
    memset(&g_profile, 0, sizeof g_profile);

    g_profile.raw_plist = (char *)malloc(plist_len + 1);
    if (!g_profile.raw_plist) {
        munmap(map, (size_t)st.st_size);
        return AFROS_ERROR_NO_MEMORY;
    }
    memcpy(g_profile.raw_plist, xml, plist_len);
    g_profile.raw_plist[plist_len] = '\0';
    munmap(map, (size_t)st.st_size);

    char *appid = extract_plist_value(g_profile.raw_plist, plist_len,
                                      "application-identifier");
    if (appid) {
        /* application-identifier is <TeamID>.<BundleID>             */
        char *dot = strchr(appid, '.');
        if (dot) {
            size_t team_len = (size_t)(dot - appid);
            if (team_len >= sizeof g_profile.team_id)
                team_len = sizeof g_profile.team_id - 1;
            memcpy(g_profile.team_id, appid, team_len);
            g_profile.team_id[team_len] = '\0';
            const char *bundle = dot + 1;
            strncpy(g_profile.app_id, bundle, sizeof g_profile.app_id - 1);
        }
        free(appid);
    }

    g_profile.creation_date[0] = '\0';
    char *created = extract_plist_value(g_profile.raw_plist, plist_len,
                                        "CreationDate");
    if (created) {
        strncpy(g_profile.creation_date, created,
                sizeof g_profile.creation_date - 1);
        free(created);
    }
    char *exp = extract_plist_value(g_profile.raw_plist, plist_len,
                                    "ExpirationDate");
    if (exp) {
        strncpy(g_profile.expiration_date, exp,
                sizeof g_profile.expiration_date - 1);
        free(exp);
    }
    g_profile.entitlements_xml = strdup(g_profile.raw_plist);
    g_profile.valid = true;
    return AFROS_SUCCESS;
}

afros_status_t ProvisionGetAppId(char *buf, size_t len) {
    if (!buf || !len) return AFROS_ERROR_INVALID_PARAM;
    if (!g_profile.valid) return AFROS_ERROR;
    strncpy(buf, g_profile.app_id, len - 1);
    buf[len - 1] = '\0';
    return AFROS_SUCCESS;
}

afros_status_t ProvisionGetTeamId(char *buf, size_t len) {
    if (!buf || !len) return AFROS_ERROR_INVALID_PARAM;
    if (!g_profile.valid) return AFROS_ERROR;
    strncpy(buf, g_profile.team_id, len - 1);
    buf[len - 1] = '\0';
    return AFROS_SUCCESS;
}

afros_status_t ProvisionGetCreationDate(char *buf, size_t len) {
    if (!buf || !len) return AFROS_ERROR_INVALID_PARAM;
    if (!g_profile.valid) return AFROS_ERROR;
    strncpy(buf, g_profile.creation_date, len - 1);
    buf[len - 1] = '\0';
    return AFROS_SUCCESS;
}

afros_status_t ProvisionGetExpirationDate(char *buf, size_t len) {
    if (!buf || !len) return AFROS_ERROR_INVALID_PARAM;
    if (!g_profile.valid) return AFROS_ERROR;
    strncpy(buf, g_profile.expiration_date, len - 1);
    buf[len - 1] = '\0';
    return AFROS_SUCCESS;
}

const char *ProvisionRawPlist(void) {
    return g_profile.valid ? g_profile.raw_plist : NULL;
}

bool ProvisionIsValid(void) {
    return g_profile.valid;
}

afros_status_t ProvisionUnload(void) {
    free(g_profile.entitlements_xml);
    free(g_profile.raw_plist);
    memset(&g_profile, 0, sizeof g_profile);
    return AFROS_SUCCESS;
}
