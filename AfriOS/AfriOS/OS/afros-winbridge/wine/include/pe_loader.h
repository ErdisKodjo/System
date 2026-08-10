#ifndef AFROS_WINBRIDGE_PE_LOADER_H
#define AFROS_WINBRIDGE_PE_LOADER_H

/*
 * pe_loader.h — API publique du chargeur PE/COFF de afros-winbridge.
 *
 * Déclare les structures et fonctions exposées par wine/pe_loader:
 *   - pe_parser.c     : analyse des en-têtes PE/COFF.
 *   - pe_to_elf.c     : mapping des sections PE en mémoire avec bonnes perms.
 *   - dll_resolver.c  : résolution des noms de DLL vers les impl. AfriOS.
 *   - resource_loader.c : chargement des ressources PE.
 */

#include "wine_compat.h"

/* --- En-tête DOS -------------------------------------------------------- */

#pragma pack(push, 1)
typedef struct _IMAGE_DOS_HEADER {
    WORD   e_magic;
    WORD   e_cblp;
    WORD   e_cp;
    WORD   e_crlc;
    WORD   e_cparhdr;
    WORD   e_minalloc;
    WORD   e_maxalloc;
    WORD   e_ss;
    WORD   e_sp;
    WORD   e_csum;
    WORD   e_ip;
    WORD   e_cs;
    WORD   e_lfarlc;
    WORD   e_ovno;
    WORD   e_res[4];
    WORD   e_oemid;
    WORD   e_oeminfo;
    WORD   e_res2[10];
    LONG   e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;
#pragma pack(pop)

#define IMAGE_DOS_SIGNATURE 0x5A4D   /* "MZ" */
#define IMAGE_NT_SIGNATURE  0x00004550 /* "PE\0\0" */

#define IMAGE_FILE_MACHINE_I386  0x014C
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_MACHINE_ARM   0x01C0
#define IMAGE_FILE_MACHINE_ARM64 0xAA64

/* --- En-tête de section ------------------------------------------------- */

#pragma pack(push, 1)
typedef struct _IMAGE_SECTION_HEADER {
    BYTE  Name[8];
    ULONG VirtualSize;
    ULONG VirtualAddress;
    ULONG SizeOfRawData;
    ULONG PointerToRawData;
    ULONG PointerToRelocations;
    ULONG PointerToLinenumbers;
    WORD  NumberOfRelocations;
    WORD  NumberOfLinenumbers;
    ULONG Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;
#pragma pack(pop)

#define IMAGE_SCN_MEM_EXECUTE  0x20000000
#define IMAGE_SCN_MEM_READ     0x40000000
#define IMAGE_SCN_MEM_WRITE    0x80000000

/* --- Descripteur de module PE chargé ----------------------------------- */

typedef struct _PE_MODULE {
    void               *base;          /* adresse virtuelle de mappage */
    ULONG               size;          /* taille totale en mémoire */
    ULONG               entry_point;   /* RVA du point d'entrée */
    ULONG               image_size;
    WORD                machine;
    BOOL                is_dll;
    char                name[32];
    IMAGE_SECTION_HEADER *sections;
    WORD                num_sections;
} PE_MODULE, *PPE_MODULE;

/* --- Entrées import/export --------------------------------------------- */

typedef struct _PE_IMPORT_ENTRY {
    char dll[64];
    char func[128];
    BOOL by_ordinal;
    ULONG ordinal;
} PE_IMPORT_ENTRY, *PPE_IMPORT_ENTRY;

typedef struct _PE_EXPORT_ENTRY {
    char   name[128];
    ULONG  ordinal;
    ULONG  rva;
} PE_EXPORT_ENTRY, *PPE_EXPORT_ENTRY;

/* --- API pe_parser.c --------------------------------------------------- */

PE_MODULE *PeLoadFromFile(const char *path);
PE_MODULE *PeLoadFromMemory(const void *data, ULONG size);
ULONG      PeGetEntryPoint(PE_MODULE *mod);
DWORD      PeGetExports(PE_MODULE *mod, PE_EXPORT_ENTRY *out, DWORD max_count);
DWORD      PeGetImports(PE_MODULE *mod, PE_IMPORT_ENTRY *out, DWORD max_count);

/* --- API pe_to_elf.c --------------------------------------------------- */

NTSTATUS PeMapToMemory(PE_MODULE *mod);
NTSTATUS PeApplyRelocations(PE_MODULE *mod);

/* --- API dll_resolver.c ----------------------------------------------- */

HANDLE  DllLoad(const char *dll_name);
void   *DllGetProc(HANDLE hmod, const char *proc_name);
const char *DllResolve(const char *dll_name);

/* --- API resource_loader.c -------------------------------------------- */

#define RT_CURSOR       1
#define RT_BITMAP       2
#define RT_ICON         3
#define RT_MENU         4
#define RT_DIALOG       5
#define RT_STRING       6
#define RT_FONTDIR      7
#define RT_FONT         8
#define RT_ACCELERATOR  9
#define RT_RCDATA       10
#define RT_VERSION      16

typedef struct _PE_RESOURCE {
    ULONG type;
    ULONG id;
    const void *data;
    ULONG size;
} PE_RESOURCE, *PPE_RESOURCE;

PE_RESOURCE *ResourceFindEx(HANDLE hmod, ULONG type, ULONG id);
DWORD        ResourceLoadString(HANDLE hmod, ULONG id, LPWSTR buf, DWORD buf_max);
PE_RESOURCE *ResourceLoadDialog(HANDLE hmod, ULONG id);

#endif /* AFROS_WINBRIDGE_PE_LOADER_H */
