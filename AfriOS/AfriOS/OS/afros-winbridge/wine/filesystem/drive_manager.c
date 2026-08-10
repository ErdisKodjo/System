/*
 * drive_manager.c — Gestionnaire de lettres de lecteur pour afros-winbridge.
 *
 * Maintient une table de correspondance entre les lettres de lecteur
 * Win32 (A:, B:, C:, D:, ...) et les chemins Unix sous-jacents. Permet
 * le montage/démontage et l'énumération.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DRIVES 26   /* A: → Z: */

typedef struct _DRIVE_ENTRY {
    BOOL  mounted;
    char  letter;            /* 'C' par ex. */
    char  unix_root[256];    /* chemin Unix monté */
    BOOL  readonly;
} DRIVE_ENTRY;

static DRIVE_ENTRY g_drives[MAX_DRIVES];

/* --- Helpers locaux ---------------------------------------------------- */

/* Retourne l'index d'une lettre de lecteur (0..25), -1 si invalide. */
static int drive_index(char letter)
{
    char c = letter;
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    return -1;
}

/* --- API publique ------------------------------------------------------ */

/* Monte un chemin Unix sur une lettre de lecteur.
 * letter est 'C', 'D', etc. Retourne AFROS_SUCCESS ou un code d'erreur.
 */
NTSTATUS DriveMount(char letter, const char *unix_path, BOOL readonly)
{
    int idx;
    if (!unix_path) return STATUS_INVALID_PARAMETER;
    idx = drive_index(letter);
    if (idx < 0) return STATUS_INVALID_PARAMETER;
    if (g_drives[idx].mounted) return STATUS_OBJECT_NAME_COLLISION;
    g_drives[idx].mounted  = TRUE;
    g_drives[idx].letter   = (letter >= 'a' && letter <= 'z')
                              ? letter - 32 : letter;
    strncpy(g_drives[idx].unix_root, unix_path,
            sizeof(g_drives[idx].unix_root) - 1);
    g_drives[idx].unix_root[sizeof(g_drives[idx].unix_root) - 1] = '\0';
    g_drives[idx].readonly = readonly;
    return STATUS_SUCCESS;
}

/* Démonte la lettre de lecteur donnée. */
NTSTATUS DriveUnmount(char letter)
{
    int idx = drive_index(letter);
    if (idx < 0) return STATUS_INVALID_PARAMETER;
    if (!g_drives[idx].mounted) return STATUS_NOT_FOUND;
    memset(&g_drives[idx], 0, sizeof(DRIVE_ENTRY));
    return STATUS_SUCCESS;
}

/* Retourne le chemin Unix racine pour une lettre montée, ou NULL. */
const char *DriveGetRoot(char letter)
{
    int idx = drive_index(letter);
    if (idx < 0) return NULL;
    if (!g_drives[idx].mounted) return NULL;
    return g_drives[idx].unix_root;
}

/* Énumère les lecteurs montés.
 * Remplit letters[] (jusqu'à max_count) et retourne le nombre réel.
 */
DWORD DriveEnum(char *letters, DWORD max_count)
{
    DWORD count = 0;
    int i;
    if (!letters || max_count == 0) return 0;
    for (i = 0; i < MAX_DRIVES && count < max_count; i++) {
        if (g_drives[i].mounted) {
            letters[count++] = g_drives[i].letter;
        }
    }
    return count;
}

/* Vérifie si un lecteur est en lecture seule. */
BOOL DriveIsReadOnly(char letter)
{
    int idx = drive_index(letter);
    if (idx < 0) return FALSE;
    if (!g_drives[idx].mounted) return FALSE;
    return g_drives[idx].readonly;
}

/* Initialise les lecteurs par défaut du système. */
NTSTATUS DriveManagerInit(void)
{
    memset(g_drives, 0, sizeof(g_drives));
    DriveMount('C', "/usr/lib/wine", FALSE);
    DriveMount('Z', "/", TRUE);
    DriveMount('D', "/mnt/cdrom", TRUE);
    return STATUS_SUCCESS;
}
