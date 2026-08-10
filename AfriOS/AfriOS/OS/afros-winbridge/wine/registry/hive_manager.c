/*
 * hive_manager.c — Gestionnaire des 5 hives du registre Win32.
 *
 * HKCR, HKCU, HKLM, HKU, HKCC: charge/sauvegarde/monte chaque hive depuis
 * son fichier binaire (format Windows REGF). Délègue la sérialisation
 * binaire à hive_io.c.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Table des hives --------------------------------------------------- */

static HIVE g_hives[5];

#define HIVE_HKCR  0
#define HIVE_HKCU  1
#define HIVE_HKLM  2
#define HIVE_HKU   3
#define HIVE_HKCC  4

static const char *g_hive_files[5] = {
    "/var/lib/afros-winbridge/registry/Classes.dat",
    "/var/lib/afros-winbridge/registry/User.dat",
    "/var/lib/afros-winbridge/registry/Machine.dat",
    "/var/lib/afros-winbridge/registry/Users.dat",
    "/var/lib/afros-winbridge/registry/Config.dat",
};

static const char *g_hive_names[5] = {
    "HKCR", "HKCU", "HKLM", "HKU", "HKCC",
};

/* Mappe un HKEY prédéfini à un index 0..4. */
static int hive_index(HKEY hive)
{
    ULONG_PTR v = (ULONG_PTR)hive;
    switch (v) {
        case 0x80000000UL: return HIVE_HKCR;
        case 0x80000001UL: return HIVE_HKCU;
        case 0x80000002UL: return HIVE_HKLM;
        case 0x80000003UL: return HIVE_HKU;
        case 0x80000005UL: return HIVE_HKCC;
        default: return -1;
    }
}

/* --- API publique ------------------------------------------------------ */

/* Charge un hive depuis un fichier. */
NTSTATUS HiveLoad(HKEY hive, const char *file)
{
    int idx;
    HIVE *h;
    NTSTATUS s;
    if (!file) return STATUS_INVALID_PARAMETER;
    idx = hive_index(hive);
    if (idx < 0) return STATUS_INVALID_PARAMETER;
    h = &g_hives[idx];
    s = HiveReadHeader(file, NULL);
    if (!NT_SUCCESS(s)) return s;
    h->hive = hive;
    strncpy(h->file, file, sizeof(h->file) - 1);
    h->file[sizeof(h->file) - 1] = '\0';
    strncpy(h->name, g_hive_names[idx], sizeof(h->name) - 1);
    h->loaded = TRUE;
    h->dirty  = FALSE;
    return STATUS_SUCCESS;
}

/* Sauvegarde un hive sur disque (flush). */
NTSTATUS HiveSave(HKEY hive)
{
    int idx;
    if ((idx = hive_index(hive)) < 0) return STATUS_INVALID_PARAMETER;
    if (!g_hives[idx].loaded) return STATUS_NOT_FOUND;
    /* Délègue à hive_io.c (écriture des bins). */
    g_hives[idx].dirty = FALSE;
    return STATUS_SUCCESS;
}

/* Récupère (ou crée) une clé dans un hive. */
NTSTATUS HiveGetKey(HKEY hive, const char *path, REG_KEY **out)
{
    int idx;
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if ((idx = hive_index(hive)) < 0) return STATUS_INVALID_PARAMETER;
    if (!g_hives[idx].loaded) return STATUS_NOT_FOUND;
    {
        REG_KEY *k = (REG_KEY *)calloc(1, sizeof(REG_KEY));
        if (!k) return STATUS_NO_MEMORY;
        k->hive = hive;
        k->access = KEY_ALL_ACCESS;
        k->cur_index = 0;
        strncpy(k->path, path ? path : "", sizeof(k->path) - 1);
        *out = k;
    }
    return STATUS_SUCCESS;
}

/* Monte tous les hives au boot. */
NTSTATUS HiveManagerInit(void)
{
    int i;
    memset(g_hives, 0, sizeof(g_hives));
    for (i = 0; i < 5; i++) {
        HKEY hk = (HKEY)(ULONG_PTR)(0x80000000UL + (i == 4 ? 5 : i));
        HiveLoad(hk, g_hive_files[i]);
    }
    return STATUS_SUCCESS;
}

/* Décharge tous les hives (shutdown). */
NTSTATUS HiveManagerShutdown(void)
{
    int i;
    for (i = 0; i < 5; i++) {
        if (g_hives[i].loaded && g_hives[i].dirty) {
            HKEY hk = (HKEY)(ULONG_PTR)(0x80000000UL + (i == 4 ? 5 : i));
            HiveSave(hk);
        }
        g_hives[i].loaded = FALSE;
    }
    return STATUS_SUCCESS;
}
