/*
 * ntfs_emulation.c — Émulation des fonctionnalités NTFS pour afros-winbridge.
 *
 * Fournit:
 *   - Alternate Data Streams (ADS): stockés comme "<file>:<stream>".
 *   - File IDs: identifiants 64-bit uniques par fichier (hash inode+device).
 *   - Journaling: stub d'interface USN pour compatibilité API.
 *
 * Sur le disque Unix, les ADS sont simulés par des fichiers cachés nommés
 * "<file>:<stream>" dans le même répertoire.
 */

#include "../include/wine_compat.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADS_SEP ':'
#define MAX_STREAM_NAME 64

/* --- Helpers locaux ---------------------------------------------------- */

/* Construit le nom Unix d'un ADS: "file.txt:streamname". */
static void ads_unix_path(char *out, size_t out_sz,
                          const char *file, const char *stream)
{
    snprintf(out, out_sz, "%s%c%s", file, ADS_SEP, stream);
}

/* --- Alternate Data Streams ------------------------------------------- */

/* Crée (ou tronque) un ADS nommé sur un fichier. Retourne un handle fd. */
HANDLE NtfsCreateAds(const char *file, const char *stream)
{
    char path[1024];
    int  fd;
    if (!file || !stream) return NULL;
    ads_unix_path(path, sizeof(path), file, stream);
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    return (fd < 0) ? NULL : (HANDLE)(LONG_PTR)fd;
}

/* Lit jusqu'à buf_max octets depuis un ADS. Retourne le nombre lu. */
DWORD NtfsReadAds(HANDLE ads, void *buf, DWORD buf_max)
{
    int fd;
    ssize_t n;
    if (!ads || !buf || buf_max == 0) return 0;
    fd = (int)(LONG_PTR)ads;
    n = read(fd, buf, buf_max);
    return (n < 0) ? 0 : (DWORD)n;
}

/* Écrit len octets dans un ADS. Retourne le nombre écrit. */
DWORD NtfsWriteAds(HANDLE ads, const void *buf, DWORD len)
{
    int fd;
    ssize_t n;
    if (!ads || !buf || len == 0) return 0;
    fd = (int)(LONG_PTR)ads;
    n = write(fd, buf, len);
    return (n < 0) ? 0 : (DWORD)n;
}

/* Ferme un handle ADS. */
void NtfsCloseAds(HANDLE ads)
{
    if (ads) close((int)(LONG_PTR)ads);
}

/* --- File IDs ---------------------------------------------------------- */

/* Calcule un File ID 64-bit unique (hash de device + inode). */
ULONGLONG NtfsGetFileId(const char *path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0;
    /* Combine dev (16 bits hauts) et inode (48 bits bas). */
    return ((ULONGLONG)(st.st_dev & 0xFFFF) << 48) |
           ((ULONGLONG)st.st_ino & 0xFFFFFFFFFFFFULL);
}

/* Récupère le File ID d'un fd déjà ouvert. */
ULONGLONG NtfsGetFileIdByFd(int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0) return 0;
    return ((ULONGLONG)(st.st_dev & 0xFFFF) << 48) |
           ((ULONGLONG)st.st_ino & 0xFFFFFFFFFFFFULL);
}

/* --- Journaling (stub USN) -------------------------------------------- */

/* Initialise le journal USN pour un volume. Stub: enregistre un en-tête
 * minimal dans <volume>/.usn-journal. */
NTSTATUS NtfsJournalInit(const char *volume_root)
{
    char path[1024];
    FILE *f;
    if (!volume_root) return STATUS_INVALID_PARAMETER;
    snprintf(path, sizeof(path), "%s/.usn-journal", volume_root);
    f = fopen(path, "wb");
    if (!f) return STATUS_ACCESS_DENIED;
    /* En-tête USN minimal: identifiant + magic + taille. */
    fputs("AFROS-USN\0", f);
    fclose(f);
    return STATUS_SUCCESS;
}

/* Énumère les enregistrements USN récents (stub: retourne 0 entrées). */
DWORD NtfsJournalEnum(const char *volume_root, void *buf, DWORD buf_max)
{
    (void)volume_root; (void)buf; (void)buf_max;
    return 0;
}
