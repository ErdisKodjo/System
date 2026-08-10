/*
 * pe_to_elf.c — Conversion des attributs de section PE vers permissions ELF.
 *
 * Mappe les sections d'un module PE en mémoire avec les bonnes permissions
 * (R/W/X) déduites des flags IMAGE_SCN_MEM_*. Applique ensuite les
 * relocations de base (IMAGE_BASE delta) pour les modules non chargés à
 * leur adresse préférée.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5

/* En-têtes NT suffisants pour la relocation (déclaration locale). */
#pragma pack(push, 1)
typedef struct _PE_FILE_HEADER {
    WORD  Machine;
    WORD  NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD  SizeOfOptionalHeader;
    WORD  Characteristics;
} PE_FILE_HEADER;

typedef struct _PE_NT_HEADERS32 {
    DWORD           Signature;
    PE_FILE_HEADER  FileHeader;
    struct {
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
        DWORD DataDirectory[16];
    } OptionalHeader;
} PE_NT_HEADERS32;
#pragma pack(pop)

/* Convertit IMAGE_SCN_MEM_* → prot flags mmap (PROT_READ/WRITE/EXEC). */
static int pe_scn_to_prot(ULONG characteristics)
{
    int prot = PROT_READ; /* par défaut lecture seule */
    if (characteristics & IMAGE_SCN_MEM_WRITE) prot |= PROT_WRITE;
    if (characteristics & IMAGE_SCN_MEM_EXECUTE) prot |= PROT_EXEC;
    if ((characteristics & (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE |
                            IMAGE_SCN_MEM_EXECUTE)) == 0)
        prot = PROT_READ;
    return prot;
}

/* --- API publique ------------------------------------------------------ */

/* Mappe toutes les sections du module PE en mémoire.
 * Alloue une région de SizeOfImage, copie les en-têtes et chaque section
 * à son VirtualAddress, puis applique mprotect() avec les perms correctes.
 */
NTSTATUS PeMapToMemory(PE_MODULE *mod)
{
    ULONG total;
    BYTE *img;
    WORD  i;
    const BYTE *src;

    if (!mod || !mod->base) return STATUS_INVALID_PARAMETER;
    total = mod->image_size ? mod->image_size : mod->size;
    total = (total + 0xFFF) & ~0xFFFul; /* align page */

    /* mmap anonyme: on obtient une zone zéro-initialisée. */
    img = (BYTE *)mmap(NULL, total, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (img == (BYTE *)MAP_FAILED) return STATUS_NO_MEMORY;

    /* Recopie les en-têtes (DOS + NT + sections) au début. */
    {
        ULONG hdr = mod->size < total ? mod->size : total;
        memcpy(img, mod->base, hdr);
    }

    /* Copie section par section à son VirtualAddress. */
    for (i = 0; i < mod->num_sections; i++) {
        IMAGE_SECTION_HEADER *s = &mod->sections[i];
        if (s->SizeOfRawData == 0) continue;
        if (s->PointerToRawData + s->SizeOfRawData > mod->size) continue;
        if (s->VirtualAddress + s->SizeOfRawData > total) continue;
        src = (const BYTE *)mod->base + s->PointerToRawData;
        memcpy(img + s->VirtualAddress, src, s->SizeOfRawData);
    }

    /* Applique les protections par section. */
    for (i = 0; i < mod->num_sections; i++) {
        IMAGE_SECTION_HEADER *s = &mod->sections[i];
        ULONG vsize = s->VirtualSize ? s->VirtualSize : s->SizeOfRawData;
        ULONG off   = s->VirtualAddress & ~0xFFFUL;
        ULONG end   = (s->VirtualAddress + vsize + 0xFFF) & ~0xFFFUL;
        if (end <= off) continue;
        mprotect(img + off, end - off, pe_scn_to_prot(s->Characteristics));
    }

    mod->base = img;
    mod->size = total;
    return STATUS_SUCCESS;
}

/* Applique les relocations PE de type IMAGE_BASE_RELLOCATION.
 * Seuls les types ABS (skip) et HIGHLOW (32-bit delta) sont gérés ici,
 * suffisants pour la majorité des binaires x86.
 */
NTSTATUS PeApplyRelocations(PE_MODULE *mod)
{
    const BYTE *base;
    const DWORD *data_dir;
    const IMAGE_DOS_HEADER *dos;
    const PE_NT_HEADERS32 *nt;
    const BYTE *reloc_blk;
    ULONG reloc_size, delta;
    ULONG old_base = 0x400000; /* base préférée typique */

    if (!mod || !mod->base) return STATUS_INVALID_PARAMETER;
    base = (const BYTE *)mod->base;
    dos  = (const IMAGE_DOS_HEADER *)base;
    nt   = (const PE_NT_HEADERS32 *)(base + dos->e_lfanew);
    data_dir = nt->OptionalHeader.DataDirectory;
    reloc_size = data_dir[IMAGE_DIRECTORY_ENTRY_BASERELOC * 2 + 1];
    if (reloc_size == 0) return STATUS_SUCCESS;

    delta = (ULONG)((ULONG_PTR)mod->base - old_base);
    if (delta == 0) return STATUS_SUCCESS;

    reloc_blk = base + data_dir[IMAGE_DIRECTORY_ENTRY_BASERELOC * 2];
    while (reloc_size > 0) {
        ULONG page_rva = *(const ULONG *)reloc_blk;
        ULONG blk_size = *((const ULONG *)reloc_blk + 1);
        const WORD *entries = (const WORD *)(reloc_blk + 8);
        ULONG i, n = (blk_size - 8) / sizeof(WORD);
        for (i = 0; i < n; i++) {
            WORD e = entries[i];
            int type = (e >> 12) & 0xF;
            int off  = e & 0xFFF;
            ULONG *target;
            if (type == 0) continue; /* IMAGE_REL_BASED_ABSOLUTE */
            if (type != 3) continue; /* HIGHLOW seulement */
            target = (ULONG *)(base + page_rva + off);
            *target += delta;
        }
        reloc_blk += blk_size;
        reloc_size -= blk_size;
    }
    return STATUS_SUCCESS;
}
