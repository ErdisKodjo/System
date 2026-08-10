/**
 * @file arc_implementation.c
 * @brief Automatic Reference Counting primitives.
 *
 * Maintains a per-object reference count embedded in the NSObject
 * header, plus a side-table for weak references.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/* Reference count layout                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    volatile int refcount;
    void        *isa;
} object_header_t;

static object_header_t *header(void *obj) {
    return (object_header_t *)obj;
}

/* ------------------------------------------------------------------ */
/* Autorelease pool                                                    */
/* ------------------------------------------------------------------ */

#define AFROS_AUTORELEASE_STACK 1024
static void  *g_autorelease_stack[AFROS_AUTORELEASE_STACK];
static size_t g_autorelease_top = 0;
static pthread_mutex_t g_ar_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct autorelease_pool_s {
    size_t mark;
    struct autorelease_pool_s *prev;
} autorelease_pool_t;

static autorelease_pool_t *g_pool_stack = NULL;

void *objc_autorelease_pool_push(void) {
    pthread_mutex_lock(&g_ar_lock);
    autorelease_pool_t *p = (autorelease_pool_t *)malloc(sizeof *p);
    p->mark = g_autorelease_top;
    p->prev = g_pool_stack;
    g_pool_stack = p;
    pthread_mutex_unlock(&g_ar_lock);
    return p;
}

void objc_autorelease_pool_pop(void *token) {
    autorelease_pool_t *p = (autorelease_pool_t *)token;
    if (!p) return;
    pthread_mutex_lock(&g_ar_lock);
    while (g_autorelease_top > p->mark) {
        void *obj = g_autorelease_stack[--g_autorelease_top];
        if (obj) objc_release(obj);
    }
    if (g_pool_stack == p) g_pool_stack = p->prev;
    free(p);
    pthread_mutex_unlock(&g_ar_lock);
}

/* ------------------------------------------------------------------ */
/* ARC primitives                                                      */
/* ------------------------------------------------------------------ */

void *objc_retain(void *obj) {
    if (!obj) return NULL;
    if (objc_is_tagged_pointer(obj)) return obj; /* tagged pointers are constant */
    __sync_fetch_and_add(&header(obj)->refcount, 1);
    return obj;
}

void objc_release(void *obj) {
    if (!obj) return;
    if (objc_is_tagged_pointer(obj)) return;
    int prev = __sync_fetch_and_sub(&header(obj)->refcount, 1);
    if (prev == 1) {
        /* Last reference: invoke -dealloc.                          */
        objc_msg_send(obj, "dealloc", NULL, NULL);
        /* Mark slot as freed to help catch double-releases.          */
        header(obj)->refcount = -1;
    }
}

void *objc_autorelease(void *obj) {
    if (!obj) return NULL;
    if (objc_is_tagged_pointer(obj)) return obj;
    pthread_mutex_lock(&g_ar_lock);
    if (g_autorelease_top < AFROS_AUTORELEASE_STACK) {
        g_autorelease_stack[g_autorelease_top++] = obj;
    }
    pthread_mutex_unlock(&g_ar_lock);
    return obj;
}

unsigned objc_retain_count(void *obj) {
    if (!obj) return 0;
    return (unsigned)header(obj)->refcount;
}

/* ------------------------------------------------------------------ */
/* Weak references via a side table                                    */
/* ------------------------------------------------------------------ */

#define AFROS_WEAK_TABLE_BITS 8
#define AFROS_WEAK_TABLE_SIZE (1u << AFROS_WEAK_TABLE_BITS)
#define AFROS_WEAK_TABLE_MASK (AFROS_WEAK_TABLE_SIZE - 1)

typedef struct weak_entry_s {
    void                  **slot;
    void                   *value;
    struct weak_entry_s    *next;
} weak_entry_t;

static weak_entry_t *g_weak_table[AFROS_WEAK_TABLE_SIZE];
static pthread_mutex_t g_weak_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t weak_hash(void **slot) {
    uintptr_t v = (uintptr_t)slot * 2654435761u;
    return (uint32_t)(v & AFROS_WEAK_TABLE_MASK);
}

afros_status_t objc_store_weak(void **slot, void *value) {
    if (!slot) return AFROS_ERROR_INVALID_PARAM;
    uint32_t h = weak_hash(slot);
    pthread_mutex_lock(&g_weak_lock);
    /* Replace existing entry if any.                                 */
    weak_entry_t *e = g_weak_table[h];
    while (e) {
        if (e->slot == slot) {
            if (e->value) objc_release(e->value);
            e->value = value;
            if (value) objc_retain(value);
            *slot = value;
            pthread_mutex_unlock(&g_weak_lock);
            return AFROS_SUCCESS;
        }
        e = e->next;
    }
    e = (weak_entry_t *)calloc(1, sizeof *e);
    if (!e) {
        pthread_mutex_unlock(&g_weak_lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    e->slot  = slot;
    e->value = value;
    e->next  = g_weak_table[h];
    g_weak_table[h] = e;
    if (value) objc_retain(value);
    *slot = value;
    pthread_mutex_unlock(&g_weak_lock);
    return AFROS_SUCCESS;
}

afros_status_t objc_load_weak(void **slot, void **out) {
    if (!slot || !out) return AFROS_ERROR_INVALID_PARAM;
    *out = *slot;
    return AFROS_SUCCESS;
}

void objc_weak_clear(void **slot) {
    if (!slot) return;
    uint32_t h = weak_hash(slot);
    pthread_mutex_lock(&g_weak_lock);
    weak_entry_t **pp = &g_weak_table[h];
    while (*pp) {
        if ((*pp)->slot == slot) {
            weak_entry_t *victim = *pp;
            *pp = victim->next;
            if (victim->value) objc_release(victim->value);
            free(victim);
            *slot = NULL;
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&g_weak_lock);
}

/* ------------------------------------------------------------------ */
/* Strong / unretained store helpers                                   */
/* ------------------------------------------------------------------ */

afros_status_t objc_store_strong(void **slot, void *value) {
    if (!slot) return AFROS_ERROR_INVALID_PARAM;
    if (value)  objc_retain(value);
    if (*slot)  objc_release(*slot);
    *slot = value;
    return AFROS_SUCCESS;
}

afros_status_t objc_autorelease_return(void *obj) {
    /* Equivalent to objc_autorelease — the compiler emits this for    */
    /* non-ARC callers returning a +1 object.                          */
    return (afros_status_t)(uintptr_t)objc_autorelease(obj);
}
