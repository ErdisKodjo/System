/*
 * hive_io.c — Lecture/écriture du format binaire des hives Windows.
 *
 * Format REGF: base block (4096 octets) + hive bins ("hbin"). Chaque bin
 * contient des cellules de taille variable (key nodes, values, etc.).
 *
 * Implémentation minimaliste: parse l'en-tête, scanne les bins et expose
 * des primitives de lecture/écriture de cellule.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* --- Constantes du format REGF ---------------------------------------- */

#define REGF_SIGNATURE   0x66676572   /* "regf" */
#define HBIN_SIGNATURE   0x6E696268   /* "hbin" */
#define REGF_BLOCK_SIZE  4096

#pragma pack(push, 1)
typedef struct _REGF_HEADER {
    DWORD signature;
    DWORD primary_sequence;
    DWORD secondary_sequence;
    DWORD last_modified;
    DWORD major_version;
    DWORD minor_version;
    DWORD file_type;
    DWORD file_format;
    DWORD root_cell_offset;
    DWORD hive_bins_data_size;
    DWORD clustering_factor;
    char  file_name[64];
    /* padding à 4096 octets... */
} REGF_HEADER;
#pragma pack(pop)

/* --- API publique ------------------------------------------------------ */

/* Lit l'en-tête REGF d'un fichier hive. */
NTSTATUS HiveReadHeader(const char *file, void *header_out)
{
    FILE *f;
    REGF_HEADER hdr;
    if (!file) return STATUS_INVALID_PARAMETER;
    f = fopen(file, "rb");
    if (!f) return STATUS_OBJECT_NAME_NOT_FOUND;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return STATUS_UNSUCCESSFUL;
    }
    fclose(f);
    if (hdr.signature != REGF_SIGNATURE)
        return STATUS_INVALID_PARAMETER; /* pas un hive valide */
    if (header_out)
        memcpy(header_out, &hdr, sizeof(hdr));
    return STATUS_SUCCESS;
}

/* Ouvre un hive en lecture/écriture et retourne un handle opaque. */
HANDLE HiveOpen(const char *file)
{
    FILE *f = file ? fopen(file, "r+b") : NULL;
    return f ? (HANDLE)f : NULL;
}

/* Ferme un handle hive. */
void HiveClose(HANDLE hh)
{
    if (hh) fclose((FILE *)hh);
}

/* Lit une cellule à un offset donné dans le hive. */
NTSTATUS HiveReadCell(HANDLE hive_handle, ULONG cell_off, void *buf, ULONG *size)
{
    FILE *f;
    LONG  len;
    if (!hive_handle || !buf || !size) return STATUS_INVALID_PARAMETER;
    f = (FILE *)hive_handle;
    /* Saute l'en-tête REGF de 4096 octets. */
    if (fseek(f, REGF_BLOCK_SIZE + cell_off, SEEK_SET) != 0)
        return STATUS_UNSUCCESSFUL;
    if (fread(&len, sizeof(len), 1, f) != 1)
        return STATUS_END_OF_FILE;
    /* Le signe indique si la cellule est utilisée (négatif) ou libre. */
    {
        ULONG actual = (ULONG)(len < 0 ? -len : len);
        if (actual > *size) actual = *size;
        if (fread(buf, 1, actual, f) != actual)
            return STATUS_BUFFER_TOO_SMALL;
        *size = actual;
    }
    return STATUS_SUCCESS;
}

/* Écrit (ou met à jour) une cellule à un offset donné. */
NTSTATUS HiveWriteCell(HANDLE hive_handle, ULONG cell_off,
                       const void *data, ULONG size)
{
    FILE *f;
    LONG  len_field;
    if (!hive_handle || !data) return STATUS_INVALID_PARAMETER;
    f = (FILE *)hive_handle;
    if (fseek(f, REGF_BLOCK_SIZE + cell_off, SEEK_SET) != 0)
        return STATUS_UNSUCCESSFUL;
    /* Taille négative = cellule utilisée. */
    len_field = -(LONG)size;
    if (fwrite(&len_field, sizeof(len_field), 1, f) != 1)
        return STATUS_UNSUCCESSFUL;
    if (fwrite(data, 1, size, f) != size)
        return STATUS_UNSUCCESSFUL;
    fflush(f);
    return STATUS_SUCCESS;
}

/* Alloue une nouvelle cellule (retourne l'offset). Stub: append. */
NTSTATUS HiveAllocateCell(HANDLE hive_handle, ULONG size, ULONG *off_out)
{
    FILE *f;
    long end;
    if (!hive_handle || !off_out) return STATUS_INVALID_PARAMETER;
    f = (FILE *)hive_handle;
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    *off_out = (ULONG)(end - REGF_BLOCK_SIZE);
    return HiveWriteCell(hive_handle, *off_out, "\0", size);
}
