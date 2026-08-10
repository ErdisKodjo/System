/*
 * activex_support.c — Hôte de contrôles ActiveX pour afros-winbridge.
 *
 * Implémente un site IOleControl minimal pour héberger des contrôles
 * ActiveX (principalement pour les pages web et les boîtes de dialogue
 * MFC qui attendent AtlAxCreateControl).
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Vtables OLE/ActiveX simplifiées --------------------------------- */

typedef struct IOleControl   IOleControl;
typedef struct IOleClientSite IOleClientSite;
typedef struct IOleInPlaceSite IOleInPlaceSite;
typedef struct IOleObject    IOleObject;
typedef struct IUnknown      IUnknown;

struct IUnknown {
    HRESULT (*QueryInterface)(IUnknown *, const void *, void **);
    ULONG   (*AddRef)(IUnknown *);
    ULONG   (*Release)(IUnknown *);
};

struct IOleControl {
    HRESULT (*QueryInterface)(IOleControl *, const void *, void **);
    ULONG   (*AddRef)(IOleControl *);
    ULONG   (*Release)(IOleControl *);
    HRESULT (*GetControlInfo)(IOleControl *, void *);
    HRESULT (*OnMnemonic)(IOleControl *, void *);
    HRESULT (*OnAmbientPropertyChange)(IOleControl *, LONG);
    HRESULT (*FreezeEvents)(IOleControl *, BOOL);
};

struct IOleClientSite {
    HRESULT (*QueryInterface)(IOleClientSite *, const void *, void **);
    ULONG   (*AddRef)(IOleClientSite *);
    ULONG   (*Release)(IOleClientSite *);
    HRESULT (*SaveObject)(IOleClientSite *);
    HRESULT (*GetMoniker)(IOleClientSite *, DWORD, DWORD, void **);
    HRESULT (*GetContainer)(IOleClientSite *, void **);
    HRESULT (*ShowObject)(IOleClientSite *);
    HRESULT (*OnShowWindow)(IOleClientSite *, BOOL);
    HRESULT (*RequestNewObjectLayout)(IOleClientSite *);
};

/* --- Site ActiveX interne -------------------------------------------- */

typedef struct _ATL_SITE {
    IOleClientSite vtbl;
    IOleObject    *control;
    char           clsid[40];
    HWND           hwnd;
    LONG           refcount;
} ATL_SITE;

/* Implémentation IOleClientSite. */
static HRESULT site_qi(IOleClientSite *self, const void *iid, void **out)
{
    (void)iid; *out = self; ((ATL_SITE *)self)->refcount++; return 0;
}
static ULONG site_addref(IOleClientSite *self)
{ return (ULONG)(++((ATL_SITE *)self)->refcount); }
static ULONG site_release(IOleClientSite *self)
{
    ATL_SITE *s = (ATL_SITE *)self;
    if (--s->refcount <= 0) { free(s); return 0; }
    return (ULONG)s->refcount;
}
static HRESULT site_save(IOleClientSite *s) { (void)s; return 0; }
static HRESULT site_get_moniker(IOleClientSite *s, DWORD a, DWORD b, void **o)
{ (void)s; (void)a; (void)b; *o = NULL; return 0x80004001L; }
static HRESULT site_get_container(IOleClientSite *s, void **o)
{ (void)s; *o = NULL; return 0x80004002L; }
static HRESULT site_show(IOleClientSite *s) { (void)s; return 0; }
static HRESULT site_on_show(IOleClientSite *s, BOOL f) { (void)s; (void)f; return 0; }
static HRESULT site_request_layout(IOleClientSite *s) { (void)s; return 0; }

/* --- API publique ------------------------------------------------------ */

/* Crée un contrôle ActiveX par CLSID et l'attache à une fenêtre. */
HRESULT AtlAxCreateControl(const char *clsid, HWND hwnd, void **out_site)
{
    ATL_SITE *s;
    if (!clsid) return 0x80070057L;
    s = (ATL_SITE *)calloc(1, sizeof(ATL_SITE));
    if (!s) return 0x8007000EL;
    s->vtbl.QueryInterface         = site_qi;
    s->vtbl.AddRef                 = site_addref;
    s->vtbl.Release                = site_release;
    s->vtbl.SaveObject             = site_save;
    s->vtbl.GetMoniker             = site_get_moniker;
    s->vtbl.GetContainer           = site_get_container;
    s->vtbl.ShowObject             = site_show;
    s->vtbl.OnShowWindow           = site_on_show;
    s->vtbl.RequestNewObjectLayout = site_request_layout;
    strncpy(s->clsid, clsid, sizeof(s->clsid) - 1);
    s->hwnd      = hwnd;
    s->refcount  = 1;
    /* En pratique: instancier le contrôle via ComCreateInstance. */
    if (out_site) *out_site = s;
    return 0; /* S_OK */
}

/* Active le contrôle in-place. */
HRESULT AtlAxAttachControl(void *control, HWND hwnd, void **out_site)
{
    (void)control; (void)hwnd;
    if (out_site) *out_site = NULL;
    return 0;
}

/* Détruit le site ActiveX. */
void AtlAxDestroySite(void *site)
{
    if (site) {
        ATL_SITE *s = (ATL_SITE *)site;
        s->refcount = 1;
        site_release(&s->vtbl);
    }
}

/* Récupère le contrôle hébergé par un site. */
HRESULT AtlAxGetControl(void *site, void **out_control)
{
    ATL_SITE *s = (ATL_SITE *)site;
    if (!s || !out_control) return 0x80070057L;
    *out_control = s->control;
    return 0;
}
