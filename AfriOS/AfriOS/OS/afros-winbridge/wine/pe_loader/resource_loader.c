/*
 * resource_loader.c — Chargement des ressources PE (RT_STRING, RT_DIALOG,
 * RT_BITMAP, ...) dans le gestionnaire de ressources de afros-winbridge.
 *
 * Lit le répertoire de ressources d'un module PE et fournit des helpers
 * spécialisés pour récupérer chaînes, dialogues et ressources génériques.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Structures internes du répertoire de ressources ------------------ */
#pragma pack(push, 1)
typedef struct _IMAGE_RESOURCE_DIRECTORY {
    DWORD Characteristics, TimeDateStamp;
    WORD  MajorVersion, MinorVersion;
    WORD  NumberOfNamedEntries, NumberOfIdEntries;
} IMAGE_RESOURCE_DIRECTORY;

typedef struct _IMAGE_RESOURCE_DIRECTORY_ENTRY {
    DWORD Name;
    DWORD OffsetToData;
} IMAGE_RESOURCE_DIRECTORY_ENTRY;

typedef struct _IMAGE_RESOURCE_DATA_ENTRY {
    DWORD OffsetToData, Size, CodePage, Reserved;
} IMAGE_RESOURCE_DATA_ENTRY;
#pragma pack(pop)

#define RES_DIR_FLAG 0x80000000u
#define IMAGE_DIRECTORY_ENTRY_RESOURCE 2

/* Extrait le data directory i depuis un PE_MODULE. */
static const DWORD *pe_data_dir(PE_MODULE *m, int idx)
{
    const BYTE *base = (const BYTE *)m->base;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const BYTE *nt = base + dos->e_lfanew;
    /* NT headers + 4 + 20 + 96 octets → data dir[0]. */
    const DWORD *dir = (const DWORD *)(nt + 24 + 96);
    (void)idx;
    return dir;
}

/* Trouve une entrée de ressource par (type, id) dans le sous-arbre. */
static const IMAGE_RESOURCE_DATA_ENTRY *res_lookup(PE_MODULE *m, ULONG type, ULONG id)
{
    const BYTE *base = (const BYTE *)m->base;
    const DWORD *dir = pe_data_dir(m, IMAGE_DIRECTORY_ENTRY_RESOURCE);
    ULONG rva = dir[IMAGE_DIRECTORY_ENTRY_RESOURCE * 2];
    ULONG sz   = dir[IMAGE_DIRECTORY_ENTRY_RESOURCE * 2 + 1];
    const BYTE *root;
    const IMAGE_RESOURCE_DIRECTORY *d;
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *e;
    WORD n, i;

    if (rva == 0 || sz == 0) return NULL;
    root = base + rva;

    /* Niveau 1: type. */
    d = (const IMAGE_RESOURCE_DIRECTORY *)root;
    n = d->NumberOfNamedEntries + d->NumberOfIdEntries;
    e = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(d + 1);
    for (i = 0; i < n; i++) {
        if ((e[i].Name & RES_DIR_FLAG) == 0 && e[i].Name == type) {
            /* Niveau 2: id. */
            const IMAGE_RESOURCE_DIRECTORY *d2 =
                (const IMAGE_RESOURCE_DIRECTORY *)(root + (e[i].OffsetToData & ~RES_DIR_FLAG));
            WORD n2 = d2->NumberOfNamedEntries + d2->NumberOfIdEntries;
            const IMAGE_RESOURCE_DIRECTORY_ENTRY *e2 =
                (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(d2 + 1);
            WORD j;
            for (j = 0; j < n2; j++) {
                if ((e2[j].Name & RES_DIR_FLAG) == 0 && e2[j].Name == id) {
                    /* Niveau 3: data entry. */
                    if (e2[j].OffsetToData & RES_DIR_FLAG) return NULL;
                    return (const IMAGE_RESOURCE_DATA_ENTRY *)(root + e2[j].OffsetToData);
                }
            }
        }
    }
    return NULL;
}

/* --- API publique ------------------------------------------------------ */

/* Recherche générique d'une ressource par type et id. */
PE_RESOURCE *ResourceFindEx(HANDLE hmod, ULONG type, ULONG id)
{
    PE_MODULE *m = (PE_MODULE *)hmod;
    static PE_RESOURCE res;
    const IMAGE_RESOURCE_DATA_ENTRY *de;
    const BYTE *base;

    if (!m) return NULL;
    de = res_lookup(m, type, id);
    if (!de) return NULL;
    base = (const BYTE *)m->base;
    res.type = type;
    res.id   = id;
    res.data = base + de->OffsetToData;
    res.size = de->Size;
    return &res;
}

/* Charge une chaîne Win32 depuis la table RT_STRING.
 * Les chaînes sont groupées par blocs de 16; id_block = (id - 1) / 16.
 * Retourne le nombre de caractères copiés (sans le NUL final).
 */
DWORD ResourceLoadString(HANDLE hmod, ULONG id, LPWSTR buf, DWORD buf_max)
{
    PE_MODULE *m = (PE_MODULE *)hmod;
    ULONG block = (id - 1) / 16;
    ULONG idx   = (id - 1) % 16;
    const IMAGE_RESOURCE_DATA_ENTRY *de;
    const BYTE *base;
    const WORD *p;
    WORD len;
    DWORD i;

    if (!m || !buf || buf_max == 0) return 0;
    de = res_lookup(m, RT_STRING, block + 1);
    if (!de) return 0;
    base = (const BYTE *)m->base;
    p = (const WORD *)(base + de->OffsetToData);
    /* Saute les idx premières chaînes. */
    for (i = 0; i < idx; i++) {
        len = *p++;
        p += len;
    }
    len = *p++;
    if (len >= buf_max) len = (WORD)(buf_max - 1);
    for (i = 0; i < len; i++) buf[i] = (wchar_t)p[i];
    buf[len] = L'\0';
    return len;
}

/* Charge un template de dialogue par id. */
PE_RESOURCE *ResourceLoadDialog(HANDLE hmod, ULONG id)
{
    return ResourceFindEx(hmod, RT_DIALOG, id);
}
