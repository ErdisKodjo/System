/*
 * kernel_objects.c — Table globale des objets noyau Win32.
 *
 * Gère les events, mutexes, semaphores, sections et timers avec un
 * handle table partagée. Fournit les primitives Ke*() attendues par
 * le sous-système Win32.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_HANDLES 1024

/* --- Types d'objets noyau -------------------------------------------- */

typedef enum _KO_TYPE {
    KO_TYPE_INVALID = 0,
    KO_TYPE_EVENT,
    KO_TYPE_MUTEX,
    KO_TYPE_SEMAPHORE,
    KO_TYPE_SECTION,
    KO_TYPE_TIMER,
} KO_TYPE;

typedef struct _KO_EVENT {
    BOOL           manual_reset;
    BOOL           signaled;
    pthread_cond_t cond;
    pthread_mutex_t lock;
} KO_EVENT;

typedef struct _KO_MUTEX {
    pthread_mutex_t lock;
    BOOL            held;
    pthread_t       owner;
    int             recurse;
} KO_MUTEX;

typedef struct _KO_SEMAPHORE {
    int             count;
    int             max;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} KO_SEMAPHORE;

typedef struct _KO_TIMER {
    ULONGLONG       due_time_ms;
    BOOL            periodic;
    ULONG           period_ms;
    BOOL            signaled;
} KO_TIMER;

typedef struct _KO_ENTRY {
    KO_TYPE  type;
    void    *obj;
    int      refcount;
} KO_ENTRY;

static KO_ENTRY         g_table[MAX_HANDLES];
static pthread_mutex_t  g_table_lock = PTHREAD_MUTEX_INITIALIZER;

/* Alloue un slot dans la handle table. */
static HANDLE ko_alloc(KO_TYPE type, void *obj)
{
    int i;
    pthread_mutex_lock(&g_table_lock);
    for (i = 1; i < MAX_HANDLES; i++) {
        if (g_table[i].type == KO_TYPE_INVALID) {
            g_table[i].type     = type;
            g_table[i].obj      = obj;
            g_table[i].refcount = 1;
            pthread_mutex_unlock(&g_table_lock);
            return (HANDLE)(LONG_PTR)(i + 0x1000);
        }
    }
    pthread_mutex_unlock(&g_table_lock);
    return NULL;
}

/* Récupère un slot par handle. */
static KO_ENTRY *ko_lookup(HANDLE h)
{
    ULONG idx = (ULONG)(ULONG_PTR)h - 0x1000;
    if (idx == 0 || idx >= MAX_HANDLES) return NULL;
    if (g_table[idx].type == KO_TYPE_INVALID) return NULL;
    return &g_table[idx];
}

/* --- API publique: Events -------------------------------------------- */

HANDLE KeCreateEvent(BOOL manual_reset, BOOL initial_state)
{
    KO_EVENT *e = (KO_EVENT *)calloc(1, sizeof(KO_EVENT));
    if (!e) return NULL;
    e->manual_reset = manual_reset;
    e->signaled     = initial_state;
    pthread_cond_init(&e->cond, NULL);
    pthread_mutex_init(&e->lock, NULL);
    return ko_alloc(KO_TYPE_EVENT, e);
}

NTSTATUS KeSetEvent(HANDLE h)
{
    KO_ENTRY *e = ko_lookup(h);
    KO_EVENT *ev;
    if (!e || e->type != KO_TYPE_EVENT) return STATUS_INVALID_HANDLE;
    ev = (KO_EVENT *)e->obj;
    pthread_mutex_lock(&ev->lock);
    ev->signaled = TRUE;
    pthread_cond_broadcast(&ev->cond);
    pthread_mutex_unlock(&ev->lock);
    return STATUS_SUCCESS;
}

NTSTATUS KeResetEvent(HANDLE h)
{
    KO_ENTRY *e = ko_lookup(h);
    KO_EVENT *ev;
    if (!e || e->type != KO_TYPE_EVENT) return STATUS_INVALID_HANDLE;
    ev = (KO_EVENT *)e->obj;
    pthread_mutex_lock(&ev->lock);
    ev->signaled = FALSE;
    pthread_mutex_unlock(&ev->lock);
    return STATUS_SUCCESS;
}

DWORD KeWaitForSingleObject(HANDLE h, DWORD timeout_ms)
{
    KO_ENTRY *e = ko_lookup(h);
    if (!e) return WAIT_FAILED;
    if (e->type == KO_TYPE_EVENT) {
        KO_EVENT *ev = (KO_EVENT *)e->obj;
        DWORD result = WAIT_OBJECT_0;
        pthread_mutex_lock(&ev->lock);
        while (!ev->signaled) {
            if (timeout_ms == INFINITE) {
                pthread_cond_wait(&ev->cond, &ev->lock);
            } else {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec  += timeout_ms / 1000;
                ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
                if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
                if (pthread_cond_timedwait(&ev->cond, &ev->lock, &ts) != 0) {
                    result = WAIT_TIMEOUT;
                    break;
                }
            }
        }
        if (!ev->manual_reset) ev->signaled = FALSE;
        pthread_mutex_unlock(&ev->lock);
        return result;
    }
    return WAIT_OBJECT_0;
}

/* --- API publique: Mutexes ------------------------------------------- */

HANDLE KeCreateMutex(void)
{
    KO_MUTEX *m = (KO_MUTEX *)calloc(1, sizeof(KO_MUTEX));
    if (!m) return NULL;
    pthread_mutex_init(&m->lock, NULL);
    m->held = FALSE; m->recurse = 0;
    return ko_alloc(KO_TYPE_MUTEX, m);
}

NTSTATUS KeReleaseMutex(HANDLE h)
{
    KO_ENTRY *e = ko_lookup(h);
    KO_MUTEX *m;
    if (!e || e->type != KO_TYPE_MUTEX) return STATUS_INVALID_HANDLE;
    m = (KO_MUTEX *)e->obj;
    pthread_mutex_lock(&m->lock);
    if (m->recurse > 0) {
        m->recurse--;
        if (m->recurse == 0) {
            m->held = FALSE;
            pthread_mutex_unlock(&m->lock);
        }
    } else {
        pthread_mutex_unlock(&m->lock);
    }
    return STATUS_SUCCESS;
}

NTSTATUS KeCloseHandle(HANDLE h)
{
    KO_ENTRY *e = ko_lookup(h);
    if (!e) return STATUS_INVALID_HANDLE;
    e->refcount--;
    if (e->refcount <= 0) {
        e->type = KO_TYPE_INVALID;
        free(e->obj);
        e->obj = NULL;
    }
    return STATUS_SUCCESS;
}
