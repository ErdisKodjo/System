/*
 * pe_parser.c — Analyse des en-têtes PE/COFF pour afros-winbridge.
 *
 * Implémente le décodage du format PE/COFF: en-tête DOS, en-tête NT,
 * table des sections, répertoires d'import et d'export. Fournit les
 * points d'entrée PeLoadFromFile(), PeLoadFromMemory(), PeGetEntryPoint(),
 * PeGetExports(), PeGetImports().
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* En-têtes internes du format PE (volontairement simplifiés). */
#pragma pack(push, 1)
typedef struct _IMAGE_FILE_HEADER {
    WORD  Machine;
    WORD  NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD  SizeOfOptionalHeader;
    WORD  Characteristics;
} IMAGE_FILE_HEADER;

typedef struct _IMAGE_OPTIONAL_HEADER32 {
    WORD  Magic;
    BYTE  MajorLinkerVersion, MinorLinkerVersion;
    DWORD SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    DWORD AddressOfEntryPoint, BaseOfCode, BaseOfData;
    DWORD ImageBase, SectionAlignment, FileAlignment;
    WORD  MajorOperatingSystemVersion, MinorOperatingSystemVersion;
    WORD  MajorImageVersion, MinorImageVersion;
    WORD  MajorSubsystemVersion, MinorSubsystemVersion;
    DWORD Win32VersionValue, SizeOfImage, SizeOfHeaders, CheckSum;
    WORD  Subsystem, DllCharacteristics;
    DWORD SizeOfStackReserve, SizeOfStackCommit;
    DWORD SizeOfHeapReserve, SizeOfHeapCommit;
    DWORD LoaderFlags, NumberOfRvaAndSizes;
    /* DataDirectory[16] suit mais nous n'avons pas besoin de le déclarer. */
} IMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_NT_HEADERS32 {
    DWORD              Signature;
    IMAGE_FILE_HEADER  FileHeader;
    IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32;

typedef struct _IMAGE_EXPORT_DIRECTORY {
    DWORD Characteristics, TimeDateStamp;
    WORD  MajorVersion, MinorVersion;
    DWORD Name, Base;
    DWORD NumberOfFunctions, NumberOfNames;
    DWORD AddressOfFunctions, AddressOfNames, AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY;

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
    DWORD OriginalFirstThunk, TimeDateStamp;
    DWORD ForwarderChain, Name, FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR;
#pragma pack(pop)

#define IMAGE_OPTIONAL_HDR32_MAGIC 0x10b
#define IMAGE_DIRECTORY_ENTRY_EXPORT    0
#define IMAGE_DIRECTORY_ENTRY_IMPORT    1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE  2

/* --- Helpers locaux ---------------------------------------------------- */

static BYTE *pe_rva_to_ptr(PE_MODULE *m, ULONG rva)
{
    WORD i;
    for (i = 0; i < m->num_sections; i++) {
        IMAGE_SECTION_HEADER *s = &m->sections[i];
        if (rva >= s->VirtualAddress &&
            rva <  s->VirtualAddress + (s->VirtualSize ? s->VirtualSize : s->SizeOfRawData))
            return (BYTE *)m->base + s->PointerToRawData +
                   (rva - s->VirtualAddress);
    }
    return NULL;
}

/* Alloue et remplit un PE_MODULE à partir d'un buffer mémoire. */
static PE_MODULE *pe_alloc(const void *data, ULONG size)
{
    PE_MODULE *m;
    const BYTE *p = (const BYTE *)data;
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    DWORD nt_off;

    if (!data || size < sizeof(IMAGE_DOS_HEADER)) return NULL;
    dos = (const IMAGE_DOS_HEADER *)p;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    nt_off = (DWORD)dos->e_lfanew;
    if (nt_off + sizeof(IMAGE_NT_HEADERS32) > size) return NULL;
    nt = (const IMAGE_NT_HEADERS32 *)(p + nt_off);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    m = (PE_MODULE *)calloc(1, sizeof(PE_MODULE));
    if (!m) return NULL;
    m->base       = (void *)p;
    m->size       = size;
    m->machine    = nt->FileHeader.Machine;
    m->is_dll     = (nt->FileHeader.Characteristics & 0x2000) ? TRUE : FALSE;
    m->entry_point = nt->OptionalHeader.AddressOfEntryPoint;
    m->image_size  = nt->OptionalHeader.SizeOfImage;
    m->num_sections = nt->FileHeader.NumberOfSections;
    m->sections = (IMAGE_SECTION_HEADER *)calloc(m->num_sections,
                                                 sizeof(IMAGE_SECTION_HEADER));
    if (!m->sections) { free(m); return NULL; }
    memcpy(m->sections,
           p + nt_off + sizeof(IMAGE_NT_HEADERS32),
           m->num_sections * sizeof(IMAGE_SECTION_HEADER));
    return m;
}

/* --- API publique ------------------------------------------------------ */

/* Charge un module PE depuis un fichier du disque. */
PE_MODULE *PeLoadFromFile(const char *path)
{
    FILE *f;
    long  fsz;
    BYTE *buf;
    PE_MODULE *m;

    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    fsz = ftell(f);
    if (fsz <= 0 || fsz > (64 * 1024 * 1024)) { fclose(f); return NULL; }
    rewind(f);
    buf = (BYTE *)malloc((size_t)fsz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)fsz, f) != (size_t)fsz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    m = pe_alloc(buf, (ULONG)fsz);
    if (!m) { free(buf); return NULL; }
    /* Le module gardeOwnership du buffer. */
    strncpy(m->name, path, sizeof(m->name) - 1);
    return m;
}

/* Charge un module PE déjà présent en mémoire. */
PE_MODULE *PeLoadFromMemory(const void *data, ULONG size)
{
    return pe_alloc(data, size);
}

/* Retourne le RVA du point d'entrée du module. */
ULONG PeGetEntryPoint(PE_MODULE *mod)
{
    return mod ? mod->entry_point : 0;
}

/* Énumère jusqu'à max_count exports du module. Retourne le nombre copié. */
DWORD PeGetExports(PE_MODULE *mod, PE_EXPORT_ENTRY *out, DWORD max_count)
{
    const IMAGE_NT_HEADERS32 *nt;
    const BYTE *base;
    const DWORD *data_dir;
    const IMAGE_EXPORT_DIRECTORY *exp;
    const DWORD *names, *funcs;
    const WORD  *ordinals;
    DWORD i, count = 0;

    if (!mod || !out || max_count == 0) return 0;
    base = (const BYTE *)mod->base;
    nt = (const IMAGE_NT_HEADERS32 *)(base +
         ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    data_dir = (const DWORD *)&nt->OptionalHeader.NumberOfRvaAndSizes + 1;
    if (data_dir[IMAGE_DIRECTORY_ENTRY_EXPORT * 2] == 0) return 0;
    exp = (const IMAGE_EXPORT_DIRECTORY *)(base + data_dir[1]);
    if (!exp) return 0;
    names     = (const DWORD *)(base + exp->AddressOfNames);
    ordinals  = (const WORD  *)(base + exp->AddressOfNameOrdinals);
    funcs     = (const DWORD *)(base + exp->AddressOfFunctions);
    for (i = 0; i < exp->NumberOfNames && count < max_count; i++) {
        PE_EXPORT_ENTRY *e = &out[count++];
        strncpy(e->name, (const char *)(base + names[i]), sizeof(e->name) - 1);
        e->ordinal = exp->Base + ordinals[i];
        e->rva     = funcs[ordinals[i]];
    }
    return count;
}

/* Énumère jusqu'à max_count imports du module. Retourne le nombre copié. */
DWORD PeGetImports(PE_MODULE *mod, PE_IMPORT_ENTRY *out, DWORD max_count)
{
    const IMAGE_NT_HEADERS32 *nt;
    const BYTE *base;
    const DWORD *data_dir;
    const IMAGE_IMPORT_DESCRIPTOR *imp;
    DWORD count = 0;

    if (!mod || !out || max_count == 0) return 0;
    base = (const BYTE *)mod->base;
    nt = (const IMAGE_NT_HEADERS32 *)(base +
         ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    data_dir = (const DWORD *)&nt->OptionalHeader.NumberOfRvaAndSizes + 1;
    if (data_dir[IMAGE_DIRECTORY_ENTRY_IMPORT * 2] == 0) return 0;
    imp = (const IMAGE_IMPORT_DESCRIPTOR *)(base +
          data_dir[IMAGE_DIRECTORY_ENTRY_IMPORT * 2 + 1]);
    while (imp->Name != 0 && count < max_count) {
        const char *dll = (const char *)(base + imp->Name);
        const DWORD *thunk = (const DWORD *)(base +
            (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        while (*thunk != 0 && count < max_count) {
            PE_IMPORT_ENTRY *e = &out[count++];
            strncpy(e->dll, dll, sizeof(e->dll) - 1);
            if (*thunk & 0x80000000) {
                e->by_ordinal = TRUE;
                e->ordinal    = *thunk & 0xFFFF;
            } else {
                const BYTE *hint_name = base + *thunk;
                e->by_ordinal = FALSE;
                e->ordinal    = 0;
                strncpy(e->func, (const char *)(hint_name + 2),
                        sizeof(e->func) - 1);
            }
            thunk++;
        }
        imp++;
    }
    (void)pe_rva_to_ptr; /* symbole réservé pour usage futur */
    return count;
}
