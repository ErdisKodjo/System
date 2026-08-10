/**
 * @file objc_runtime.c
 * @brief Core Objective-C runtime: class registry, ivar layout,
 *        category merge and the top-level dispatch entry points.
 *
 * Provides a small in-process class table for the AfriOS Apple
 * compatibility layer. The runtime does NOT depend on libobjc — it
 * is a self-contained implementation usable from C.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/* Runtime structures (defined in include/afros_apple.h)               */
/* ------------------------------------------------------------------ */

#define AFROS_OBJC_MAX_CLASSES 256
static objc_class_t *g_classes[AFROS_OBJC_MAX_CLASSES];
static uint32_t      g_class_count = 0;
static pthread_mutex_t g_class_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/* Class lookup                                                        */
/* ------------------------------------------------------------------ */

objc_class_t *ObjcGetClass(const char *name) {
    if (!name) return NULL;
    pthread_mutex_lock(&g_class_lock);
    for (uint32_t i = 0; i < g_class_count; i++) {
        if (g_classes[i] && strcmp(g_classes[i]->name, name) == 0) {
            objc_class_t *c = g_classes[i];
            pthread_mutex_unlock(&g_class_lock);
            return c;
        }
    }
    pthread_mutex_unlock(&g_class_lock);
    return NULL;
}

afros_status_t ObjcRegisterClass(objc_class_t *cls) {
    if (!cls || !cls->name) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_class_lock);
    if (ObjcGetClass(cls->name) != NULL) {
        pthread_mutex_unlock(&g_class_lock);
        return AFROS_ERROR_INVALID_PARAM;
    }
    if (g_class_count >= AFROS_OBJC_MAX_CLASSES) {
        pthread_mutex_unlock(&g_class_lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    cls->registered = true;
    g_classes[g_class_count++] = cls;
    pthread_mutex_unlock(&g_class_lock);
    return AFROS_SUCCESS;
}

afros_status_t ObjcUnregisterClass(objc_class_t *cls) {
    if (!cls) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_class_lock);
    for (uint32_t i = 0; i < g_class_count; i++) {
        if (g_classes[i] == cls) {
            g_classes[i] = g_classes[--g_class_count];
            cls->registered = false;
            pthread_mutex_unlock(&g_class_lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_class_lock);
    return AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Method dispatch                                                     */
/* ------------------------------------------------------------------ */

objc_imp_t objc_lookup_imp(objc_class_t *cls, const char *sel);

/* Lookup that walks the class hierarchy (used during class setup).   */
static objc_imp_t class_lookup_imp_local(objc_class_t *cls, const char *sel) {
    for (objc_class_t *c = cls; c != NULL; c = c->super) {
        for (uint32_t i = 0; i < c->nmethods; i++) {
            if (strcmp(c->methods[i].name, sel) == 0) {
                return c->methods[i].imp;
            }
        }
        for (uint32_t k = 0; k < c->ncategories; k++) {
            objc_class_t *cat = c->categories[k];
            for (uint32_t i = 0; i < cat->nmethods; i++) {
                if (strcmp(cat->methods[i].name, sel) == 0) {
                    return cat->methods[i].imp;
                }
            }
        }
    }
    return NULL;
}

/* Public alias used by category-merge / debugging paths.              */
objc_imp_t ObjcLookupImp(objc_class_t *cls, const char *sel) {
    return class_lookup_imp_local(cls, sel);
}

/* ------------------------------------------------------------------ */
/* Ivar layout                                                         */
/* ------------------------------------------------------------------ */

objc_ivar_t *ObjcGetIvar(objc_class_t *cls, const char *name) {
    if (!cls || !name) return NULL;
    for (objc_class_t *c = cls; c; c = c->super) {
        for (uint32_t i = 0; i < c->nivars; i++) {
            if (strcmp(c->ivars[i].name, name) == 0) {
                return &c->ivars[i];
            }
        }
    }
    return NULL;
}

afros_status_t ObjcAddIvar(objc_class_t *cls, const char *name,
                          uint32_t size, const char *types) {
    if (!cls || !name) return AFROS_ERROR_INVALID_PARAM;
    objc_ivar_t *iv = (objc_ivar_t *)realloc(
        cls->ivars, (cls->nivars + 1) * sizeof(objc_ivar_t));
    if (!iv) return AFROS_ERROR_NO_MEMORY;
    cls->ivars = iv;
    iv[cls->nivars].name   = name;
    iv[cls->nivars].types  = types ? types : "";
    iv[cls->nivars].offset = (uint32_t)cls->instance_size;
    cls->instance_size    += size;
    cls->nivars++;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Category merge                                                      */
/* ------------------------------------------------------------------ */

afros_status_t ObjcAddCategory(objc_class_t *cls, objc_class_t *category) {
    if (!cls || !category) return AFROS_ERROR_INVALID_PARAM;
    objc_class_t **arr = (objc_class_t **)realloc(
        cls->categories,
        (cls->ncategories + 1) * sizeof(objc_class_t *));
    if (!arr) return AFROS_ERROR_NO_MEMORY;
    cls->categories = arr;
    cls->categories[cls->ncategories++] = category;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Class allocation                                                    */
/* ------------------------------------------------------------------ */

void *ObjcAlloc(objc_class_t *cls) {
    if (!cls) return NULL;
    void *obj = calloc(1, cls->instance_size + sizeof(objc_class_t *));
    if (!obj) return NULL;
    *((objc_class_t **)obj) = cls;
    return obj;
}

void *ObjcAllocNamed(const char *cls_name) {
    objc_class_t *cls = ObjcGetClass(cls_name);
    return cls ? ObjcAlloc(cls) : NULL;
}

afros_status_t ObjcAddMethod(objc_class_t *cls, const char *sel,
                             objc_imp_t imp, const char *types) {
    if (!cls || !sel || !imp) return AFROS_ERROR_INVALID_PARAM;
    objc_method_t *m = (objc_method_t *)realloc(
        cls->methods, (cls->nmethods + 1) * sizeof(objc_method_t));
    if (!m) return AFROS_ERROR_NO_MEMORY;
    cls->methods = m;
    cls->methods[cls->nmethods].name  = sel;
    cls->methods[cls->nmethods].imp   = imp;
    cls->methods[cls->nmethods].types = types ? types : "";
    cls->nmethods++;
    return AFROS_SUCCESS;
}

uint32_t ObjcClassCount(void) {
    return g_class_count;
}

objc_class_t **ObjcClassList(uint32_t *count) {
    if (count) *count = g_class_count;
    return g_classes;
}
