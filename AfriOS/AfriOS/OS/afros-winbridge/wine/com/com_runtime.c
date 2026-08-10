/*
 * com_runtime.c — Runtime COM pour afros-winbridge.
 *
 * Implémente CoInitialize/CoUninitialize, le class registry (CLSID → factory)
 * et CoCreateInstance. Toutes les factories sont stockées dans une table
 * statique globale.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* --- Codes HRESULT COM ----------------------------------------------- */
#define S_OK                ((HRESULT)0)
#define S_FALSE             ((HRESULT)1)
#define E_FAIL              ((HRESULT)0x80004005L)
#define E_INVALIDARG        ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY       ((HRESULT)0x8007000EL)
#define E_NOINTERFACE       ((HRESULT)0x80004002L)
#define E_POINTER           ((HRESULT)0x80004003L)
#define E_NOTIMPL           ((HRESULT)0x80004001L)
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)

/* --- Types COM simplifiés -------------------------------------------- */

typedef struct IUnknown IUnknown;
typedef struct IClassFactory IClassFactory;

typedef ULONG (*PFN_ADDREF)(IUnknown *);
typedef ULONG (*PFN_RELEASE)(IUnknown *);
typedef HRESULT (*PFN_QUERYINTERFACE)(IUnknown *, const void *iid, void **out);
typedef HRESULT (*PFN_CREATEINSTANCE)(IClassFactory *, void *outer,
                                       const void *iid, void **out);

struct IUnknown {
    PFN_QUERYINTERFACE QueryInterface;
    PFN_ADDREF         AddRef;
    PFN_RELEASE        Release;
};

struct IClassFactory {
    PFN_QUERYINTERFACE    QueryInterface;
    PFN_ADDREF            AddRef;
    PFN_RELEASE           Release;
    PFN_CREATEINSTANCE    CreateInstance;
};

/* --- Table des classes enregistrées ---------------------------------- */

typedef struct _CLASS_ENTRY {
    char           clsid[40];   /* "{...}" */
    char           progid[64];
    IClassFactory *factory;
} CLASS_ENTRY;

#define MAX_CLASSES 64
static CLASS_ENTRY       g_classes[MAX_CLASSES];
static int               g_class_count = 0;
static pthread_mutex_t   g_com_lock = PTHREAD_MUTEX_INITIALIZER;
static DWORD             g_init_count = 0;

/* --- Helpers locaux ---------------------------------------------------- */

static CLASS_ENTRY *find_class_by_clsid(const char *clsid)
{
    int i;
    for (i = 0; i < g_class_count; i++)
        if (strcmp(g_classes[i].clsid, clsid) == 0) return &g_classes[i];
    return NULL;
}

/* --- API publique ------------------------------------------------------ */

/* Initialise COM pour le thread courant. */
HRESULT ComInit(void)
{
    pthread_mutex_lock(&g_com_lock);
    g_init_count++;
    pthread_mutex_unlock(&g_com_lock);
    return S_OK;
}

/* Déinitialise COM. */
void ComUninit(void)
{
    pthread_mutex_lock(&g_com_lock);
    if (g_init_count > 0) g_init_count--;
    pthread_mutex_unlock(&g_com_lock);
}

/* Enregistre une class factory pour un CLSID donné. */
HRESULT ComRegisterClass(const char *clsid, const char *progid,
                         IClassFactory *factory)
{
    CLASS_ENTRY *e;
    if (!clsid || !factory) return E_INVALIDARG;
    pthread_mutex_lock(&g_com_lock);
    if (find_class_by_clsid(clsid)) {
        pthread_mutex_unlock(&g_com_lock);
        return E_FAIL; /* déjà enregistré */
    }
    if (g_class_count >= MAX_CLASSES) {
        pthread_mutex_unlock(&g_com_lock);
        return E_OUTOFMEMORY;
    }
    e = &g_classes[g_class_count++];
    strncpy(e->clsid, clsid, sizeof(e->clsid) - 1);
    strncpy(e->progid, progid ? progid : "", sizeof(e->progid) - 1);
    e->factory = factory;
    pthread_mutex_unlock(&g_com_lock);
    return S_OK;
}

/* Crée une instance d'un objet COM par CLSID. */
HRESULT ComCreateInstance(const char *clsid, void *outer,
                          const void *iid, void **out)
{
    CLASS_ENTRY *e;
    if (!clsid || !out) return E_INVALIDARG;
    *out = NULL;
    pthread_mutex_lock(&g_com_lock);
    e = find_class_by_clsid(clsid);
    pthread_mutex_unlock(&g_com_lock);
    if (!e || !e->factory) return REGDB_E_CLASSNOTREG;
    if (!e->factory->CreateInstance) return E_FAIL;
    return e->factory->CreateInstance(e->factory, outer, iid, out);
}

/* Recherche un CLSID par ProgID. */
HRESULT ComClsidFromProgId(const char *progid, char *clsid_out, DWORD clsid_max)
{
    int i;
    if (!progid || !clsid_out) return E_INVALIDARG;
    pthread_mutex_lock(&g_com_lock);
    for (i = 0; i < g_class_count; i++) {
        if (strcmp(g_classes[i].progid, progid) == 0) {
            strncpy(clsid_out, g_classes[i].clsid, clsid_max - 1);
            clsid_out[clsid_max - 1] = '\0';
            pthread_mutex_unlock(&g_com_lock);
            return S_OK;
        }
    }
    pthread_mutex_unlock(&g_com_lock);
    return REGDB_E_CLASSNOTREG;
}
