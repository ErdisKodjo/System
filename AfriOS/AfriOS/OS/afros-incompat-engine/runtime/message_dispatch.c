/**
 * @file message_dispatch.c
 * @brief Implementation of objc_msgSend semantics with a small IMP
 *        cache, taggable-pointer class decoding and super-dispatch.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/* IMP cache                                                           */
/* ------------------------------------------------------------------ */

#define AFROS_IMP_CACHE_BITS 7
#define AFROS_IMP_CACHE_SIZE (1u << AFROS_IMP_CACHE_BITS)
#define AFROS_IMP_CACHE_MASK (AFROS_IMP_CACHE_SIZE - 1)

typedef struct {
    objc_class_t  *cls;
    const char    *sel;
    objc_imp_t     imp;
} imp_cache_entry_t;

static imp_cache_entry_t g_imp_cache[AFROS_IMP_CACHE_SIZE];
static pthread_mutex_t   g_cache_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t hash_ptr_pair(void *a, void *b) {
    uintptr_t v = (uintptr_t)a * 2654435761u + (uintptr_t)b;
    return (uint32_t)(v & AFROS_IMP_CACHE_MASK);
}

static objc_imp_t cache_lookup(objc_class_t *cls, const char *sel) {
    uint32_t h = hash_ptr_pair(cls, (void *)sel);
    pthread_mutex_lock(&g_cache_lock);
    objc_imp_t imp = NULL;
    if (g_imp_cache[h].cls == cls && g_imp_cache[h].sel == sel) {
        imp = g_imp_cache[h].imp;
    }
    pthread_mutex_unlock(&g_cache_lock);
    return imp;
}

static void cache_insert(objc_class_t *cls, const char *sel, objc_imp_t imp) {
    uint32_t h = hash_ptr_pair(cls, (void *)sel);
    pthread_mutex_lock(&g_cache_lock);
    g_imp_cache[h].cls = cls;
    g_imp_cache[h].sel = sel;
    g_imp_cache[h].imp = imp;
    pthread_mutex_unlock(&g_cache_lock);
}

void objc_msg_flush_cache(void) {
    pthread_mutex_lock(&g_cache_lock);
    memset(g_imp_cache, 0, sizeof g_imp_cache);
    pthread_mutex_unlock(&g_cache_lock);
}

/* ------------------------------------------------------------------ */
/* Tagged-pointer class decoding                                       */
/* ------------------------------------------------------------------ */

#define AFROS_TAGGED_MASK       0x8000000000000000ULL
#define AFROS_TAGGED_CLASS_MASK 0x0000000000000f80ULL
#define AFROS_TAGGED_CLASS_SHFT 7

/* Up to 16 tagged-pointer classes are supported.                     */
static objc_class_t *g_tagged_classes[16];
static uint32_t      g_tagged_class_count = 0;

afros_status_t objc_register_tagged_class(uint8_t index, objc_class_t *cls) {
    if (index >= 16 || !cls) return AFROS_ERROR_INVALID_PARAM;
    g_tagged_classes[index] = cls;
    if ((uint32_t)index + 1 > g_tagged_class_count) {
        g_tagged_class_count = (uint32_t)index + 1;
    }
    return AFROS_SUCCESS;
}

static objc_class_t *decode_tagged(uintptr_t ptr) {
    if ((ptr & AFROS_TAGGED_MASK) == 0) return NULL;
    uint8_t idx = (uint8_t)((ptr & AFROS_TAGGED_CLASS_MASK)
                            >> AFROS_TAGGED_CLASS_SHFT);
    if (idx >= g_tagged_class_count) return NULL;
    return g_tagged_classes[idx];
}

bool objc_is_tagged_pointer(void *obj) {
    return (((uintptr_t)obj) & AFROS_TAGGED_MASK) != 0;
}

/* ------------------------------------------------------------------ */
/* Lookup that walks the class hierarchy                               */
/* ------------------------------------------------------------------ */

objc_imp_t objc_lookup_imp(objc_class_t *cls, const char *sel) {
    if (!cls || !sel) return NULL;
    objc_imp_t cached = cache_lookup(cls, sel);
    if (cached) return cached;

    for (objc_class_t *c = cls; c != NULL; c = c->super) {
        for (uint32_t i = 0; i < c->nmethods; i++) {
            if (strcmp(c->methods[i].name, sel) == 0) {
                objc_imp_t imp = c->methods[i].imp;
                if (imp) cache_insert(cls, sel, imp);
                return imp;
            }
        }
        for (uint32_t k = 0; k < c->ncategories; k++) {
            objc_class_t *cat = c->categories[k];
            for (uint32_t i = 0; i < cat->nmethods; i++) {
                if (strcmp(cat->methods[i].name, sel) == 0) {
                    objc_imp_t imp = cat->methods[i].imp;
                    if (imp) cache_insert(cls, sel, imp);
                    return imp;
                }
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* objc_msgSend entry points                                           */
/* ------------------------------------------------------------------ */

void *objc_msg_send(void *receiver, const char *sel,
                    void *arg0, void *arg1) {
    if (!receiver || !sel) return NULL;
    objc_class_t *cls = decode_tagged((uintptr_t)receiver);
    if (!cls) cls = *((objc_class_t **)receiver);
    if (!cls) return NULL;

    objc_imp_t imp = objc_lookup_imp(cls, sel);
    if (!imp) {
        objc_imp_t fwd = objc_lookup_imp(cls, "forwardingTargetForSelector:");
        if (fwd) {
            void *target = NULL;
            fwd(receiver, (void *)"forwardingTargetForSelector:",
                (void *)sel, &target);
            if (target) return objc_msg_send(target, sel, arg0, arg1);
        }
        return NULL;
    }
    void *result = NULL;
    imp(receiver, (void *)sel, arg0, arg1);
    if (arg1) result = *(void **)arg1;
    return result;
}

void *objc_msg_send_super(objc_class_t *super_cls, void *receiver,
                          const char *sel, void *arg0, void *arg1) {
    if (!super_cls || !receiver || !sel) return NULL;
    objc_imp_t imp = objc_lookup_imp(super_cls->super, sel);
    if (!imp) return NULL;
    void *result = NULL;
    imp(receiver, (void *)sel, arg0, arg1);
    if (arg1) result = *(void **)arg1;
    return result;
}

/* ------------------------------------------------------------------ */
/* Selector table                                                      */
/* ------------------------------------------------------------------ */

#define AFROS_SEL_MAX 1024
static const char *g_sel_table[AFROS_SEL_MAX];
static uint32_t    g_sel_count = 0;
static pthread_mutex_t g_sel_lock = PTHREAD_MUTEX_INITIALIZER;

const char *objc_register_selector(const char *name) {
    if (!name) return NULL;
    pthread_mutex_lock(&g_sel_lock);
    for (uint32_t i = 0; i < g_sel_count; i++) {
        if (strcmp(g_sel_table[i], name) == 0) {
            const char *s = g_sel_table[i];
            pthread_mutex_unlock(&g_sel_lock);
            return s;
        }
    }
    if (g_sel_count >= AFROS_SEL_MAX) {
        pthread_mutex_unlock(&g_sel_lock);
        return name;
    }
    g_sel_table[g_sel_count] = strdup(name);
    const char *s = g_sel_table[g_sel_count++];
    pthread_mutex_unlock(&g_sel_lock);
    return s;
}

/* Bridge to ObjcMsgSend declared in afros_apple.h.                   */
void *ObjcMsgSend(void *receiver, const char *sel, void *arg0, void *arg1) {
    return objc_msg_send(receiver, sel, arg0, arg1);
}
