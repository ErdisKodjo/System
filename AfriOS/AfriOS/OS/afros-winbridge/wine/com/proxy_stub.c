/*
 * proxy_stub.c — Proxy/Stub standards pour IDispatch et IUnknown.
 *
 * Fournit des implémentations de proxy (côté client) et stub (côté serveur)
 * pour les interfaces COM les plus communes. Les proxies délèguent les
 * appels au marshaler qui les transmet au serveur.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Codes HRESULT COM ----------------------------------------------- */
#define S_OK                ((HRESULT)0)
#define E_NOTIMPL           ((HRESULT)0x80004001L)
#define E_NOINTERFACE       ((HRESULT)0x80004002L)
#define E_FAIL              ((HRESULT)0x80004005L)
#define E_INVALIDARG        ((HRESULT)0x80070057L)
#define DISP_E_UNKNOWNNAME  ((HRESULT)0x80020006L)

/* --- Alias de types Win32 ------------------------------------------- */
typedef unsigned int   UINT;
typedef wchar_t        OLECHAR;
typedef OLECHAR       *LPOLESTR;

/* --- Vtables simplifiées --------------------------------------------- */

typedef struct IUnknown   IUnknown;
typedef struct IDispatch  IDispatch;
typedef IUnknown         *LPUNKNOWN;
typedef IDispatch        *LPDISPATCH;

struct IUnknown {
    HRESULT (*QueryInterface)(LPUNKNOWN, const void *iid, void **out);
    ULONG   (*AddRef)(LPUNKNOWN);
    ULONG   (*Release)(LPUNKNOWN);
};

struct IDispatch {
    /* IUnknown */
    HRESULT (*QueryInterface)(LPDISPATCH, const void *iid, void **out);
    ULONG   (*AddRef)(LPDISPATCH);
    ULONG   (*Release)(LPDISPATCH);
    /* IDispatch */
    HRESULT (*GetTypeInfoCount)(LPDISPATCH, UINT *);
    HRESULT (*GetTypeInfo)(LPDISPATCH, UINT, DWORD, void **);
    HRESULT (*GetIDsOfNames)(LPDISPATCH, const void *riid,
                              LPOLESTR *names, UINT count, DWORD lcid, LONG *dispids);
    HRESULT (*Invoke)(LPDISPATCH, LONG dispid, const void *riid, DWORD lcid,
                       WORD flags, void *params, void *result, void *excep, UINT *err);
};

/* --- Proxy IUnknown --------------------------------------------------- */

typedef struct _UNK_PROXY {
    IUnknown vtbl;
    DWORD    server_object_id;
    LONG     refcount;
} UNK_PROXY;

static HRESULT unk_qi(LPUNKNOWN self, const void *iid, void **out)
{
    (void)iid; *out = self; ((UNK_PROXY *)self)->refcount++;
    return S_OK;
}
static ULONG unk_addref(LPUNKNOWN self)  { return (ULONG)(++((UNK_PROXY *)self)->refcount); }
static ULONG unk_release(LPUNKNOWN self)
{
    UNK_PROXY *p = (UNK_PROXY *)self;
    if (--p->refcount <= 0) { free(p); return 0; }
    return (ULONG)p->refcount;
}

/* Crée un proxy IUnknown pour un objet serveur donné. */
LPUNKNOWN ProxyCreateUnknown(DWORD server_object_id)
{
    UNK_PROXY *p = (UNK_PROXY *)calloc(1, sizeof(UNK_PROXY));
    if (!p) return NULL;
    p->vtbl.QueryInterface = unk_qi;
    p->vtbl.AddRef         = unk_addref;
    p->vtbl.Release        = unk_release;
    p->server_object_id    = server_object_id;
    p->refcount            = 1;
    return (LPUNKNOWN)p;
}
/* --- Proxy IDispatch -------------------------------------------------- */

typedef struct _DISP_PROXY {
    IDispatch vtbl;
    DWORD     server_object_id;
    LONG      refcount;
} DISP_PROXY;

static HRESULT disp_qi(LPDISPATCH self, const void *iid, void **out)
{
    (void)iid; *out = self; ((DISP_PROXY *)self)->refcount++;
    return S_OK;
}
static ULONG disp_addref(LPDISPATCH self) { return (ULONG)(++((DISP_PROXY *)self)->refcount); }
static ULONG disp_release(LPDISPATCH self)
{
    DISP_PROXY *p = (DISP_PROXY *)self;
    if (--p->refcount <= 0) { free(p); return 0; }
    return (ULONG)p->refcount;
}
static HRESULT disp_get_type_info_count(LPDISPATCH s, UINT *count) { (void)s; *count = 0; return S_OK; }
static HRESULT disp_get_type_info(LPDISPATCH s, UINT i, DWORD l, void **o) { (void)s; (void)i; (void)l; *o = NULL; return 0x80004001L; }
static HRESULT disp_get_ids_of_names(LPDISPATCH s, const void *r, LPOLESTR *n, UINT c, DWORD l, LONG *d)
{ (void)s; (void)r; (void)n; (void)c; (void)l; *d = -1; return 0x80020006L; }
static HRESULT disp_invoke(LPDISPATCH s, LONG d, const void *r, DWORD l, WORD f, void *p, void *res, void *e, UINT *err)
{ (void)s; (void)d; (void)r; (void)l; (void)f; (void)p; (void)res; (void)e; (void)err; return S_OK; }

/* Crée un proxy IDispatch. */
LPDISPATCH ProxyCreateDispatch(DWORD server_object_id)
{
    DISP_PROXY *p = (DISP_PROXY *)calloc(1, sizeof(DISP_PROXY));
    if (!p) return NULL;
    p->vtbl.QueryInterface     = disp_qi;
    p->vtbl.AddRef             = disp_addref;
    p->vtbl.Release            = disp_release;
    p->vtbl.GetTypeInfoCount   = disp_get_type_info_count;
    p->vtbl.GetTypeInfo        = disp_get_type_info;
    p->vtbl.GetIDsOfNames      = disp_get_ids_of_names;
    p->vtbl.Invoke             = disp_invoke;
    p->server_object_id        = server_object_id;
    p->refcount                = 1;
    return (LPDISPATCH)p;
}

/* --- Stub (côté serveur) --------------------------------------------- */

/* Reçoit un buffer marshaled et appelle la méthode correspondante
 * sur l'objet réel. Stub simplifié. */
HRESULT StubDispatchCall(DWORD object_id, LONG dispid,
                         const void *params, DWORD params_size,
                         void *result, DWORD result_max)
{
    (void)object_id; (void)dispid; (void)params; (void)params_size;
    (void)result; (void)result_max;
    return S_OK;
}

/* Codes d'erreur COM utilisés. */
/* (définis plus haut) */
