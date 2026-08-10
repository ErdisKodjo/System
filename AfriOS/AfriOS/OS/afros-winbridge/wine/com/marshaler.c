/*
 * marshaler.c — Marshaling COM pour afros-winbridge.
 *
 * Sérialise les pointeurs d'interface COM en STUB data transmissible
 * entre processus (LPC/sockets) et reconstruit un proxy côté récepteur.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* --- Format de STUB data --------------------------------------------- */

#pragma pack(push, 1)
typedef struct _STUB_HEADER {
    DWORD  signature;       /* "AFMS" = AfriOS Marshal Stub */
    DWORD  version;
    char   iid[40];         /* CLSID/IID sous forme texte */
    DWORD  object_id;       /* identifiant serveur de l'objet */
    DWORD  ctx;             /* MSHCTX */
    DWORD  data_size;
} STUB_HEADER;
#pragma pack(pop)

#define STUB_SIG 0x534D4641   /* "AFMS" */

/* --- Table des objets marshés côté serveur --------------------------- */

typedef struct _MARSHALED_OBJ {
    DWORD    id;
    void    *iface;
    char     iid[40];
    int      refcount;
} MARSHALED_OBJ;

#define MAX_MARSHALED 256
static MARSHALED_OBJ  g_objs[MAX_MARSHALED];
static int            g_obj_count = 0;
static DWORD          g_next_id = 1;
static pthread_mutex_t g_marshal_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- Helpers locaux ---------------------------------------------------- */

static MARSHALED_OBJ *find_obj(DWORD id)
{
    int i;
    for (i = 0; i < g_obj_count; i++)
        if (g_objs[i].id == id) return &g_objs[i];
    return NULL;
}

static MARSHALED_OBJ *register_obj(void *iface, const char *iid)
{
    MARSHALED_OBJ *o;
    if (g_obj_count >= MAX_MARSHALED) return NULL;
    o = &g_objs[g_obj_count++];
    o->id       = g_next_id++;
    o->iface    = iface;
    o->refcount = 1;
    strncpy(o->iid, iid ? iid : "", sizeof(o->iid) - 1);
    return o;
}

/* --- API publique ------------------------------------------------------ */

/* Sérialise une interface en STUB data. Retourne la taille écrite. */
DWORD MarshalInterface(void *buf, DWORD buf_max, void *iface,
                       const char *iid, DWORD ctx)
{
    STUB_HEADER *hdr;
    MARSHALED_OBJ *o;
    DWORD total;
    if (!buf || !iface) return 0;
    pthread_mutex_lock(&g_marshal_lock);
    o = register_obj(iface, iid);
    pthread_mutex_unlock(&g_marshal_lock);
    if (!o) return 0;
    total = sizeof(STUB_HEADER);
    if (total > buf_max) return 0;
    hdr = (STUB_HEADER *)buf;
    hdr->signature = STUB_SIG;
    hdr->version   = 1;
    hdr->object_id = o->id;
    hdr->ctx       = ctx;
    hdr->data_size = 0;
    strncpy(hdr->iid, o->iid, sizeof(hdr->iid) - 1);
    hdr->iid[sizeof(hdr->iid) - 1] = '\0';
    return total;
}

/* Reconstruit un proxy depuis la STUB data. */
HRESULT UnmarshalInterface(const void *buf, DWORD buf_size, void **out)
{
    const STUB_HEADER *hdr;
    MARSHALED_OBJ *o;
    if (!buf || !out || buf_size < sizeof(STUB_HEADER)) return 0x80004005L;
    hdr = (const STUB_HEADER *)buf;
    if (hdr->signature != STUB_SIG) return 0x80004005L;
    pthread_mutex_lock(&g_marshal_lock);
    o = find_obj(hdr->object_id);
    if (o) o->refcount++;
    pthread_mutex_unlock(&g_marshal_lock);
    if (!o) return 0x80040154L; /* REGDB_E_CLASSNOTREG */
    *out = o->iface;
    return 0; /* S_OK */
}

/* Libère un objet marshé côté serveur. */
NTSTATUS MarshalRelease(DWORD object_id)
{
    MARSHALED_OBJ *o;
    NTSTATUS r = STATUS_NOT_FOUND;
    pthread_mutex_lock(&g_marshal_lock);
    o = find_obj(object_id);
    if (o) {
        o->refcount--;
        if (o->refcount <= 0) {
            /* Compacte. */
            int i;
            for (i = 0; i < g_obj_count; i++)
                if (g_objs[i].id == object_id) break;
            if (i + 1 < g_obj_count)
                g_objs[i] = g_objs[g_obj_count - 1];
            g_obj_count--;
            r = STATUS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_marshal_lock);
    return r;
}

/* Compte les objets marshés actifs (debug). */
DWORD MarshalActiveCount(void)
{
    return (DWORD)g_obj_count;
}
