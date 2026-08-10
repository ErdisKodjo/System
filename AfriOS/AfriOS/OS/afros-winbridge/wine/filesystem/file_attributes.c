/*
 * file_attributes.c — Conversion des attributs de fichiers Win32 ↔ Unix.
 *
 * Traduit les FILE_ATTRIBUTE_* Win32 vers le champ st_mode de struct stat
 * et inversement. Fournit aussi GetFileAttributesEx() qui combine stat()
 * et la traduction.
 */

#include "../include/wine_compat.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* --- API publique ------------------------------------------------------ */

/* Convertit un champ st_mode Unix en un masque FILE_ATTRIBUTE_* Win32. */
DWORD UnixAttrToWin(mode_t mode)
{
    DWORD attr = 0;

    if (S_ISDIR(mode)) attr |= FILE_ATTRIBUTE_DIRECTORY;
    if (S_ISREG(mode) && (mode & 0111) == 0) attr |= FILE_ATTRIBUTE_NORMAL;
    if (S_ISLNK(mode)) attr |= FILE_ATTRIBUTE_REPARSE_POINT;
    if ((mode & 0200) == 0) attr |= FILE_ATTRIBUTE_READONLY;
    if (S_ISCHR(mode) || S_ISBLK(mode)) attr |= FILE_ATTRIBUTE_SYSTEM;
    /* Les archives sont marquées par convention pour les fichiers réguliers. */
    if (S_ISREG(mode)) attr |= FILE_ATTRIBUTE_ARCHIVE;
    /* Fichiers cachés: convention Unix → nom commençant par '.'. */
    /* (à gérer au niveau du chemin, pas du mode.) */
    if (attr == 0) attr = FILE_ATTRIBUTE_NORMAL;
    return attr;
}

/* Convertit un masque FILE_ATTRIBUTE_* Win32 en champ st_mode Unix. */
mode_t WinAttrToUnix(DWORD attr)
{
    mode_t mode = 0;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        mode = S_IFDIR | 0755;
    } else if (attr & FILE_ATTRIBUTE_SYSTEM) {
        mode = S_IFCHR | 0600;
    } else {
        mode = S_IFREG | 0644;
    }
    if ((attr & FILE_ATTRIBUTE_READONLY) == 0) mode |= 0200;
    if (attr & FILE_ATTRIBUTE_TEMPORARY) mode |= 0100;
    return mode;
}

/* Implémentation GetFileAttributes() Win32: retourne le masque Win32
 * ou INVALID_FILE_ATTRIBUTES (0xFFFFFFFF) si le fichier n'existe pas.
 */
DWORD GetFileAttributes(const char *path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return (DWORD)0xFFFFFFFF;
    return UnixAttrToWin(st.st_mode);
}

/* GetFileAttributesEx(): récupère attributs + tailles + timestamps. */
typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
    DWORD    dwFileAttributes;
    ULARGE_INTEGER ftCreationTime;
    ULARGE_INTEGER ftLastAccessTime;
    ULARGE_INTEGER ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;

BOOL GetFileAttributesEx(const char *path,
                         WIN32_FILE_ATTRIBUTE_DATA *out)
{
    struct stat st;
    if (!path || !out) return FALSE;
    if (stat(path, &st) != 0) return FALSE;
    memset(out, 0, sizeof(*out));
    out->dwFileAttributes = UnixAttrToWin(st.st_mode);
    out->ftCreationTime.QuadPart   = (ULONGLONG)st.st_ctime * 10000000ULL + 116444736000000000ULL;
    out->ftLastAccessTime.QuadPart = (ULONGLONG)st.st_atime * 10000000ULL + 116444736000000000ULL;
    out->ftLastWriteTime.QuadPart  = (ULONGLONG)st.st_mtime * 10000000ULL + 116444736000000000ULL;
    out->nFileSizeHigh = (DWORD)((ULONGLONG)st.st_size >> 32);
    out->nFileSizeLow  = (DWORD)(st.st_size & 0xFFFFFFFF);
    return TRUE;
}

/* Marqueur d'erreur retourné par GetFileAttributes quand introuvable. */
#define INVALID_FILE_ATTRIBUTES ((DWORD)0xFFFFFFFF)
