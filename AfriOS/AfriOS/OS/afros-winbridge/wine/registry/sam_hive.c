/*
 * sam_hive.c — Hive SAM (Security Accounts Manager).
 *
 * HKLM\SAM contient les comptes utilisateurs et groupes. Pour des raisons
 * de sécurité, ce hive est accessible en lecture seule depuis l'espace
 * utilisateur Win32. On expose une API minimale pour récupérer un SID
 * utilisateur.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <string.h>

/* Comptes utilisateurs par défaut. */
typedef struct _SAM_USER {
    const char *name;
    const char *sid;        /* SID au format S-1-5-... */
    DWORD       rid;        /* Relative ID */
} SAM_USER;

static const SAM_USER g_default_users[] = {
    { "Administrator", "S-1-5-21-0-0-0-500", 500 },
    { "Guest",         "S-1-5-21-0-0-0-501", 501 },
    { "afros",         "S-1-5-21-0-0-0-1000", 1000 },
    { "SYSTEM",        "S-1-5-18", 18 },
};

#define G_DEFAULT_USER_COUNT \
    (sizeof(g_default_users) / sizeof(g_default_users[0]))

/* Groupes par défaut. */
typedef struct _SAM_GROUP {
    const char *name;
    const char *sid;
} SAM_GROUP;

static const SAM_GROUP g_default_groups[] = {
    { "Administrators", "S-1-5-32-544" },
    { "Users",          "S-1-5-32-545" },
    { "Guests",         "S-1-5-32-546" },
    { "Power Users",    "S-1-5-32-547" },
};

#define G_DEFAULT_GROUP_COUNT \
    (sizeof(g_default_groups) / sizeof(g_default_groups[0]))

/* --- API publique ------------------------------------------------------ */

/* Initialise le hive SAM en lecture seule. */
NTSTATUS SamHiveInit(void)
{
    REG_KEY *k;
    DWORD i;
    /* Enregistre les utilisateurs dans HKLM\SAM\Domains\Account\Users. */
    for (i = 0; i < G_DEFAULT_USER_COUNT; i++) {
        char path[256];
        snprintf(path, sizeof(path),
                 "SAM\\Domains\\Account\\Users\\%05lu",
                 (unsigned long)g_default_users[i].rid);
        if (NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &k))) {
            RegSetValue(k, "Name", REG_SZ, g_default_users[i].name,
                        (DWORD)strlen(g_default_users[i].name) + 1);
            RegCloseKey(k);
        }
    }
    for (i = 0; i < G_DEFAULT_GROUP_COUNT; i++) {
        char path[256];
        snprintf(path, sizeof(path), "SAM\\Domains\\Builtin\\%s",
                 g_default_groups[i].name);
        if (NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &k))) {
            RegSetValue(k, "Sid", REG_SZ, g_default_groups[i].sid,
                        (DWORD)strlen(g_default_groups[i].sid) + 1);
            RegCloseKey(k);
        }
    }
    return STATUS_SUCCESS;
}

/* Récupère le SID d'un utilisateur. */
NTSTATUS SamGetUserInfo(const char *user, char *sid_out, DWORD sid_len)
{
    DWORD i;
    if (!user || !sid_out || sid_len == 0) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < G_DEFAULT_USER_COUNT; i++) {
        if (strcmp(g_default_users[i].name, user) == 0) {
            strncpy(sid_out, g_default_users[i].sid, sid_len - 1);
            sid_out[sid_len - 1] = '\0';
            return STATUS_SUCCESS;
        }
    }
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

/* Énumère les groupes. Retourne le nombre de groupes copiés. */
DWORD SamEnumGroups(char (*names)[64], DWORD max_count)
{
    DWORD i, n = G_DEFAULT_GROUP_COUNT;
    if (!names || max_count == 0) return 0;
    if (n > max_count) n = max_count;
    for (i = 0; i < n; i++)
        strncpy(names[i], g_default_groups[i].name, 63);
    return n;
}
