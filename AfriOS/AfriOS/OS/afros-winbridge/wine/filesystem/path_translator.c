/*
 * path_translator.c — Traduction de chemins Win32 ↔ Unix pour afros-winbridge.
 *
 * Convertit les chemins Windows ("C:\\Windows\\System32\\foo.dll",
 * "Z:\\home\\user") en chemins Unix ("/usr/lib/wine/system32/foo.dll",
 * "/home/user") et inversement. Gère aussi la normalisation des
 * séparateurs et la résolution des chemins relatifs.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Préfixe Unix où sont exposés les drives Windows. */
#define WINE_PREFIX   "/usr/lib/wine"
#define MAX_PATH_WIN  260

/* --- Helpers locaux ---------------------------------------------------- */

/* Convertit tous les '\\' en '/' dans buf (in-place). */
static void backslashes_to_slashes(char *buf)
{
    char *p = buf;
    while (*p) { if (*p == '\\') *p = '/'; p++; }
}

/* Convertit tous les '/' en '\\' dans buf (in-place). */
static void slashes_to_backslashes(char *buf)
{
    char *p = buf;
    while (*p) { if (*p == '/') *p = '\\'; p++; }
}

/* Inspecte la lettre de lecteur (C:, D:, Z:). Retourne 1 si c'est Z:
 * (mappé à la racine Unix), 0 sinon. */
static int is_z_drive(char drive_letter)
{
    char c = drive_letter;
    if (c >= 'A' && c <= 'Z') c += 32;
    return c == 'z';
}

/* --- API publique ------------------------------------------------------ */

/* Traduit un chemin Windows en chemin Unix.
 * "C:\\Windows\\System32\\foo.dll" → "/usr/lib/wine/system32/foo.dll"
 * "Z:\\home\\user\\bar.txt"        → "/home/user/bar.txt"
 */
const char *WinPathToUnix(const char *win_path)
{
    static char buf[MAX_PATH_WIN * 2];
    char drive;
    const char *rest;

    if (!win_path) return NULL;
    /* Si déjà un chemin unix absolu, on le retourne tel quel. */
    if (win_path[0] == '/') return win_path;
    if (win_path[0] == '\0' || win_path[1] != ':') return NULL;
    drive = win_path[0];
    rest  = win_path + 2; /* saute "C:" */
    if (rest[0] == '\\' || rest[0] == '/') rest++;

    if (is_z_drive(drive)) {
        snprintf(buf, sizeof(buf), "/%s", rest);
    } else if (drive == 'C' || drive == 'c') {
        snprintf(buf, sizeof(buf), "%s/system32/%s", WINE_PREFIX, rest);
    } else if (drive == 'D' || drive == 'd') {
        snprintf(buf, sizeof(buf), "%s/drive_d/%s", WINE_PREFIX, rest);
    } else {
        snprintf(buf, sizeof(buf), "%s/drive_%c/%s", WINE_PREFIX,
                 drive >= 'a' ? drive : drive + 32, rest);
    }
    backslashes_to_slashes(buf);
    return buf;
}

/* Traduit un chemin unix en chemin Windows. */
const char *UnixPathToWin(const char *unix_path)
{
    static char buf[MAX_PATH_WIN * 2];
    size_t prefix_len;

    if (!unix_path) return NULL;
    prefix_len = strlen(WINE_PREFIX);
    if (strncmp(unix_path, WINE_PREFIX, prefix_len) == 0) {
        const char *rest = unix_path + prefix_len;
        if (rest[0] == '/') rest++;
        if (strncmp(rest, "system32/", 9) == 0)
            snprintf(buf, sizeof(buf), "C:\\Windows\\System32\\%s", rest + 9);
        else if (strncmp(rest, "drive_d/", 8) == 0)
            snprintf(buf, sizeof(buf), "D:\\%s", rest + 8);
        else
            snprintf(buf, sizeof(buf), "C:\\%s", rest);
    } else {
        /* Hors du prefix wine → mappé sur Z:. */
        snprintf(buf, sizeof(buf), "Z:%s", unix_path);
    }
    slashes_to_backslashes(buf);
    return buf;
}

/* Calcule le chemin complet d'un chemin Win32 possiblement relatif.
 * Si le chemin commence par une lettre de lecteur, on le retourne tel quel
 * (après normalisation). Sinon on le préfixe avec le répertoire courant
 * Win32 (C:\\windows\\system32 par défaut).
 */
const char *WinPathGetFullPath(const char *win_path)
{
    static char buf[MAX_PATH_WIN * 2];

    if (!win_path) return NULL;
    if (win_path[0] != '\0' && win_path[1] == ':') {
        /* Déjà absolu — recopie + normalise les séparateurs. */
        strncpy(buf, win_path, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        backslashes_to_slashes(buf);
        slashes_to_backslashes(buf);
        return buf;
    }
    if (win_path[0] == '\\' || win_path[0] == '/') {
        /* Chemin absolu sans lettre de drive → racine de C:. */
        snprintf(buf, sizeof(buf), "C:%s", win_path);
        slashes_to_backslashes(buf);
        return buf;
    }
    /* Relatif → C:\windows\system32\<rel>. */
    snprintf(buf, sizeof(buf), "C:\\windows\\system32\\%s", win_path);
    return buf;
}

/* Retourne le séparateur de chemin Win32 ('\\'). */
char WinPathSeparator(void)
{
    return '\\';
}
