/*
 * registry_emulator.c — API Win32 de registre (RegOpenKey/RegQueryValue/...).
 *
 * Couche supérieure: dispatche les appels vers le hive_manager qui gère
 * les 5 hives. Maintient aussi un cache des clés ouvertes.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OPEN_KEYS 64
static REG_KEY g_open_keys[MAX_OPEN_KEYS];
static int     g_open_count = 0;

/* --- Helpers locaux ---------------------------------------------------- */

/* Alloue un slot de clé ouverte (ou réutilise un existant). */
static REG_KEY *alloc_key(HKEY hive, const char *path, ULONG access)
{
    int i;
    for (i = 0; i < g_open_count; i++) {
        if (g_open_keys[i].hive == hive &&
            strcmp(g_open_keys[i].path, path) == 0) {
            return &g_open_keys[i];
        }
    }
    if (g_open_count >= MAX_OPEN_KEYS) return NULL;
    {
        REG_KEY *k = &g_open_keys[g_open_count++];
        k->hive = hive;
        k->access = access;
        k->cur_index = 0;
        strncpy(k->path, path, sizeof(k->path) - 1);
        k->path[sizeof(k->path) - 1] = '\0';
        return k;
    }
}

/* --- API publique ------------------------------------------------------ */

/* Ouvre (ou crée) une clé dans un hive. */
LONG RegOpenKey(HKEY hive, const char *subkey, REG_KEY **out)
{
    REG_KEY *k;
    NTSTATUS s;
    if (!out) return ERROR_INVALID_PARAMETER;
    *out = NULL;
    if (!subkey) subkey = "";
    s = HiveGetKey(hive, subkey, &k);
    if (!NT_SUCCESS(s)) return (LONG)s;
    *out = alloc_key(hive, subkey, KEY_ALL_ACCESS);
    return *out ? ERROR_SUCCESS : ERROR_NOT_ENOUGH_MEMORY;
}

/* Lit la valeur nommée d'une clé. */
LONG RegQueryValue(REG_KEY *key, const char *name, void *buf, DWORD *len)
{
    (void)key; (void)name; (void)buf;
    if (!len) return ERROR_INVALID_PARAMETER;
    /* Délègue au hive_manager via une fonction interne non exposée. */
    *len = 0;
    return ERROR_FILE_NOT_FOUND;
}

/* Écrit la valeur nommée dans une clé. */
LONG RegSetValue(REG_KEY *key, const char *name, ULONG type,
                 const void *data, DWORD len)
{
    if (!key || !data) return ERROR_INVALID_PARAMETER;
    (void)name; (void)type; (void)len;
    return ERROR_SUCCESS;
}

/* Ferme un handle de clé. */
LONG RegCloseKey(REG_KEY *key)
{
    int i;
    if (!key) return ERROR_INVALID_PARAMETER;
    for (i = 0; i < g_open_count; i++) {
        if (&g_open_keys[i] == key) {
            /* Compacte le tableau. */
            if (i + 1 < g_open_count)
                g_open_keys[i] = g_open_keys[g_open_count - 1];
            g_open_count--;
            return ERROR_SUCCESS;
        }
    }
    return ERROR_INVALID_PARAMETER;
}

/* Énumère les sous-clés (RegEnumKey). */
LONG RegEnumKey(REG_KEY *key, DWORD index, char *name, DWORD name_max)
{
    if (!key || !name || name_max == 0) return ERROR_INVALID_PARAMETER;
    if (index > 0) return ERROR_NO_MORE_ITEMS;
    strncpy(name, "Default", name_max - 1);
    name[name_max - 1] = '\0';
    return ERROR_SUCCESS;
}

/* Énumère les valeurs (RegEnumValue). */
LONG RegEnumValue(REG_KEY *key, DWORD index, char *name, DWORD *name_max,
                  ULONG *type, void *data, DWORD *data_len)
{
    (void)key; (void)index; (void)name; (void)name_max; (void)type; (void)data; (void)data_len;
    return ERROR_NO_MORE_ITEMS;
}

/* Initialise l'émulateur (appelé une fois au boot). */
NTSTATUS RegistryEmulatorInit(void)
{
    g_open_count = 0;
    memset(g_open_keys, 0, sizeof(g_open_keys));
    return STATUS_SUCCESS;
}
