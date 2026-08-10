/*
 * preloader.c — Préchargeur ELF de afros-winbridge.
 *
 * Le preloader s'exécute avant le wine_loader: il réserve les zones
 * de mémoire virtuelle dont Wine a besoin (adresse de base préférée
 * des DLL Windows, pile principale, heap), puis saute dans wine_loader.
 *
 * Sur AfriOS, le preloader est invoqué par le noyau comme interpréteur
 * ELF des fichiers .exe (via le champ PT_INTERP).
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>

/* --- Constantes de layout mémoire ------------------------------------ */

/* Adresse de base préférée pour les images PE (Windows user-space). */
#define PE_PREFERRED_BASE   0x00400000UL
#define PE_RESERVE_SIZE     (256UL * 1024 * 1024)  /* 256 MiB */

/* Pile principale du processus Win32. */
#define MAIN_STACK_BASE     0x7F000000UL
#define MAIN_STACK_SIZE     (8UL * 1024 * 1024)    /* 8 MiB */

/* Heap principal. */
#define MAIN_HEAP_BASE      0x00100000UL
#define MAIN_HEAP_SIZE      (16UL * 1024 * 1024)   /* 16 MiB */

/* --- Réservation de régions mémoire --------------------------------- */

/* Réserve une région anonyme (PROT_NONE) pour empêcher le kernel de
 * l'allouer ailleurs. */
static int reserve_region(unsigned long base, unsigned long size)
{
    void *p = mmap((void *)base, size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) return -1;
    return 0;
}

/* --- Variables globales exportées vers wine_loader ------------------- */

/* Layout mémoire établi par le preloader. */
typedef struct _PRELOAD_LAYOUT {
    unsigned long pe_base;
    unsigned long pe_size;
    unsigned long stack_base;
    unsigned long stack_size;
    unsigned long heap_base;
    unsigned long heap_size;
    int           argc;
    char        **argv;
} PRELOAD_LAYOUT;

PRELOAD_LAYOUT g_preload_layout;

/* --- API publique ------------------------------------------------------ */

/* Initialise le layout mémoire du processus Wine. */
NTSTATUS PreloaderSetup(int argc, char **argv)
{
    memset(&g_preload_layout, 0, sizeof(g_preload_layout));

    /* Réserve la zone de base PE. */
    if (reserve_region(PE_PREFERRED_BASE, PE_RESERVE_SIZE) != 0) {
        /* Non fatal: le loader saura s'adapter avec relocations. */
    }
    g_preload_layout.pe_base = PE_PREFERRED_BASE;
    g_preload_layout.pe_size = PE_RESERVE_SIZE;

    /* Réserve la pile principale. */
    if (reserve_region(MAIN_STACK_BASE, MAIN_STACK_SIZE) != 0) {
        /* Non fatal. */
    }
    g_preload_layout.stack_base = MAIN_STACK_BASE;
    g_preload_layout.stack_size = MAIN_STACK_SIZE;

    /* Réserve le heap. */
    if (reserve_region(MAIN_HEAP_BASE, MAIN_HEAP_SIZE) != 0) {
        /* Non fatal. */
    }
    g_preload_layout.heap_base = MAIN_HEAP_BASE;
    g_preload_layout.heap_size = MAIN_HEAP_SIZE;

    g_preload_layout.argc = argc;
    g_preload_layout.argv = argv;
    return STATUS_SUCCESS;
}

/* Point d'entrée principal du preloader: configure le layout puis saute
 * dans wine_loader. */
NTSTATUS PreloaderMain(int argc, char **argv)
{
    NTSTATUS s = PreloaderSetup(argc, argv);
    if (!NT_SUCCESS(s)) return s;
    /* En pratique: on saute directement dans WineLoaderMain() via un
     * longjmp ou un appel de fonction. Ici on délègue simplement. */
    extern NTSTATUS WineLoaderMain(int argc, char **argv);
    return WineLoaderMain(argc, argv);
}

/* Récupère le layout établi par le preloader. */
const PRELOAD_LAYOUT *PreloaderGetLayout(void)
{
    return &g_preload_layout;
}

/* Libère une région réservée (avant de la réutiliser avec d'autres perms). */
NTSTATUS PreloaderUnreserve(unsigned long base, unsigned long size)
{
    return (munmap((void *)base, size) == 0) ? STATUS_SUCCESS
                                             : STATUS_UNSUCCESSFUL;
}
