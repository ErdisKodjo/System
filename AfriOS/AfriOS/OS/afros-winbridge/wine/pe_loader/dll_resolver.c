/*
 * dll_resolver.c — Résolution des noms de DLL Win32 vers implémentations AfriOS.
 *
 * Maintient une table de correspondance { "kernel32.dll" → "libafros-kernel32.so" }
 * et un cache des modules déjà chargés via dlopen(). Expose DllResolve(),
 * DllLoad() et DllGetProc().
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Table de résolution statique ------------------------------------- */

typedef struct _DLL_MAP {
    const char *win_name;
    const char *unix_name;
} DLL_MAP;

static const DLL_MAP g_dll_map[] = {
    { "kernel32.dll",   "libafros-kernel32.so"  },
    { "user32.dll",     "libafros-user32.so"    },
    { "gdi32.dll",      "libafros-gdi32.so"     },
    { "advapi32.dll",   "libafros-advapi32.so"  },
    { "ole32.dll",      "libafros-ole32.so"     },
    { "oleaut32.dll",   "libafros-oleaut32.so"  },
    { "comctl32.dll",   "libafros-comctl32.so"  },
    { "comdlg32.dll",   "libafros-comdlg32.so"  },
    { "shell32.dll",    "libafros-shell32.so"   },
    { "ntdll.dll",      "libafros-ntdll.so"     },
    { "msvcrt.dll",     "libafros-msvcrt.so"    },
    { "ws2_32.dll",     "libafros-ws2_32.so"    },
    { "wininet.dll",    "libafros-wininet.so"   },
    { "winmm.dll",      "libafros-winmm.so"     },
    { "d3d9.dll",       "libafros-d3d9.so"      },
    { "d3d11.dll",      "libafros-d3d11.so"     },
    { "dxgi.dll",       "libafros-dxgi.so"      },
    { "dinput8.dll",    "libafros-dinput8.so"   },
    { "dsound.dll",     "libafros-dsound.so"    },
    { "version.dll",    "libafros-version.so"   },
};

#define G_DLL_MAP_COUNT (sizeof(g_dll_map) / sizeof(g_dll_map[0]))

/* --- Cache de modules chargés ----------------------------------------- */

typedef struct _DLL_ENTRY {
    char  name[32];
    void *handle;       /* dlopen handle */
    int   refcount;
} DLL_ENTRY;

#define MAX_DLL_CACHE 64
static DLL_ENTRY g_cache[MAX_DLL_CACHE];
static int       g_cache_count = 0;

/* Comparaison case-insensitive limitée à la longueur de a. */
static int dll_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* --- API publique ------------------------------------------------------ */

/* Retourne le nom de fichier Unix correspondant au nom Win32. */
const char *DllResolve(const char *dll_name)
{
    size_t i;
    if (!dll_name) return NULL;
    for (i = 0; i < G_DLL_MAP_COUNT; i++)
        if (dll_ieq(dll_name, g_dll_map[i].win_name))
            return g_dll_map[i].unix_name;
    /* Convention: si inconnu, on génère libafros-<basename>.so. */
    return NULL;
}

/* Charge une DLL Win32 via son équivalent Unix. */
HANDLE DllLoad(const char *dll_name)
{
    const char *so_name;
    DLL_ENTRY *slot = NULL;
    int i;

    if (!dll_name) return NULL;
    for (i = 0; i < g_cache_count; i++) {
        if (dll_ieq(dll_name, g_cache[i].name)) {
            g_cache[i].refcount++;
            return (HANDLE)g_cache[i].handle;
        }
    }
    if (g_cache_count >= MAX_DLL_CACHE) return NULL;
    so_name = DllResolve(dll_name);
    if (!so_name) {
        /* Fallback: chemin direct vers /usr/lib/wine/<dll_name>.dll.so. */
        static char path[256];
        snprintf(path, sizeof(path), "/usr/lib/wine/%s.dll.so", dll_name);
        so_name = path;
    }
    slot = &g_cache[g_cache_count];
    strncpy(slot->name, dll_name, sizeof(slot->name) - 1);
    slot->handle = dlopen(so_name, RTLD_NOW | RTLD_GLOBAL);
    if (!slot->handle) {
        slot->name[0] = '\0';
        return NULL;
    }
    slot->refcount = 1;
    g_cache_count++;
    return (HANDLE)slot->handle;
}

/* Récupère l'adresse d'une procédure exportée. */
void *DllGetProc(HANDLE hmod, const char *proc_name)
{
    if (!hmod || !proc_name) return NULL;
    return dlsym(hmod, proc_name);
}
