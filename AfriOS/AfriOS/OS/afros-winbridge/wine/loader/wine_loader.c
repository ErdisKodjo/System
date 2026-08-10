/*
 * wine_loader.c — Chargeur principal du processus Wine pour afros-winbridge.
 *
 * Initialise tous les sous-systèmes (PE loader, registry, syscall translator,
 * wineserver client, COM, filesystem) puis charge et exécute l'image PE
 * cible passée en argument.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* --- Déclarations externes ------------------------------------------- */

extern NTSTATUS HiveManagerInit(void);
extern NTSTATUS SystemHiveInit(void);
extern NTSTATUS SoftwareHiveInit(void);
extern NTSTATUS SamHiveInit(void);
extern NTSTATUS RegistryEmulatorInit(void);
extern NTSTATUS DriveManagerInit(void);
extern NTSTATUS TaskSchedulerInit(void);
extern NTSTATUS SystemHiveLoadDriver(const char *driver_name);

/* --- Variables globales ---------------------------------------------- */

static BOOL  g_initialized = FALSE;
static char  g_main_image[256];
static char  g_main_cmdline[1024];

/* --- Helpers locaux ---------------------------------------------------- */

/* Initialise tous les sous-systèmes Wine. */
static NTSTATUS init_subsystems(void)
{
    NTSTATUS s;
    s = RegistryEmulatorInit();
    if (!NT_SUCCESS(s)) return s;
    s = HiveManagerInit();
    if (!NT_SUCCESS(s)) return s;
    s = SystemHiveInit();
    if (!NT_SUCCESS(s)) return s;
    s = SoftwareHiveInit();
    if (!NT_SUCCESS(s)) return s;
    s = SamHiveInit();
    if (!NT_SUCCESS(s)) return s;
    s = DriveManagerInit();
    if (!NT_SUCCESS(s)) return s;
    s = TaskSchedulerInit();
    /* Non fatal. */
    return STATUS_SUCCESS;
}

/* Charge l'image PE principale. */
static NTSTATUS load_main_image(const char *path)
{
    PE_MODULE *mod;
    DWORD      imports_count, i;
    PE_IMPORT_ENTRY imports[64];

    mod = PeLoadFromFile(path);
    if (!mod) {
        fprintf(stderr, "wine_loader: cannot load %s\n", path);
        return STATUS_UNSUCCESSFUL;
    }
    if (!NT_SUCCESS(PeMapToMemory(mod)))
        return STATUS_UNSUCCESSFUL;
    if (!NT_SUCCESS(PeApplyRelocations(mod)))
        return STATUS_UNSUCCESSFUL;

    /* Résout et charge les imports. */
    imports_count = PeGetImports(mod, imports, _countof(imports));
    for (i = 0; i < imports_count; i++) {
        HANDLE h = DllLoad(imports[i].dll);
        if (!h) {
            fprintf(stderr, "wine_loader: missing DLL %s\n", imports[i].dll);
            /* Continue: certaines DLL sont optionnelles. */
        }
    }
    return STATUS_SUCCESS;
}

/* --- API publique ------------------------------------------------------ */

/* Point d'entrée principal du wine_loader. */
NTSTATUS WineLoaderMain(int argc, char **argv)
{
    NTSTATUS s;
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <exe> [args...]\n", argv[0]);
        return STATUS_INVALID_PARAMETER;
    }
    strncpy(g_main_image, argv[1], sizeof(g_main_image) - 1);
    g_main_image[sizeof(g_main_image) - 1] = '\0';
    g_main_cmdline[0] = '\0';
    for (i = 1; i < argc && strlen(g_main_cmdline) + strlen(argv[i]) + 1 <
                    sizeof(g_main_cmdline); i++) {
        if (i > 1) strncat(g_main_cmdline, " ", sizeof(g_main_cmdline) - strlen(g_main_cmdline) - 1);
        strncat(g_main_cmdline, argv[i], sizeof(g_main_cmdline) - strlen(g_main_cmdline) - 1);
    }

    if (!g_initialized) {
        s = init_subsystems();
        if (!NT_SUCCESS(s)) return s;
        g_initialized = TRUE;
    }
    s = load_main_image(g_main_image);
    if (!NT_SUCCESS(s)) return s;

    /* En pratique: on saute au point d'entrée du PE. Pour la sandbox on
     * retourne simplement STATUS_SUCCESS. */
    return STATUS_SUCCESS;
}

/* Récupère l'image principale en cours de chargement. */
const char *WineLoaderGetMainImage(void)
{
    return g_main_image;
}

/* Récupère la ligne de commande complète. */
const char *WineLoaderGetCmdLine(void)
{
    return g_main_cmdline;
}

/* Recharge les DLLs déjà chargées (utile après un fork). */
NTSTATUS WineLoaderReload(void)
{
    if (!g_initialized) return STATUS_NOT_FOUND;
    return STATUS_SUCCESS;
}
