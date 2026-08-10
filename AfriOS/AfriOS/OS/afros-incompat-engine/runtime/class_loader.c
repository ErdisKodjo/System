/**
 * @file class_loader.c
 * @brief Discover and register Objective-C classes from a loaded
 *        Mach-O image by walking the __objc_classlist section.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* On-disk ObjC class layout (subset, version 16 / 64-bit)             */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t isa;
    uint32_t super;
    uint32_t cache;
    uint32_t vtable;
    uint32_t data;     /* points to class_ro_t                          */
} objc_class_disk_t;

typedef struct {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
    uint32_t reserved;
    uint64_t ivarLayout;
    uint64_t name;
    uint64_t baseMethods;
    uint64_t baseProtocols;
    uint64_t ivars;
    uint64_t weakIvarLayout;
    uint64_t baseProperties;
} class_ro_t;

typedef struct {
    uint32_t name;
    uint32_t types;
    uint32_t imp;
    uint32_t pad;
} objc_method_disk_t;

typedef struct {
    uint32_t offset;
    uint32_t name;
    uint32_t types;
    uint32_t alignment;
    uint32_t size;
} objc_ivar_disk_t;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const char *read_cstring(macho_image_t *img, uint64_t ptr) {
    if (!ptr) return NULL;
    /* In a real image, `ptr` is a vmaddr that needs to be rebased    */
    /* by the slide. We use the loaded base directly.                  */
    const uint8_t *base = (const uint8_t *)macho_loaded_base(img);
    if (!base) return NULL;
    uint64_t lo = ~0ULL;
    segment_command_64_t *segs = NULL;
    uint32_t nsegs = 0;
    if (MachoGetSegments(img, &segs, &nsegs) != AFROS_SUCCESS) return NULL;
    const segment_command_64_t **segv = (const segment_command_64_t **)segs;
    for (uint32_t i = 0; i < nsegs; i++) {
        if (ptr >= segv[i]->vmaddr && ptr < segv[i]->vmaddr + segv[i]->vmsize) {
            uint64_t off = ptr - segv[i]->vmaddr;
            return (const char *)(base + off);
        }
        if (segv[i]->vmaddr < lo) lo = segv[i]->vmaddr;
    }
    (void)lo;
    return NULL;
}

static objc_class_t *register_from_disk(macho_image_t *img,
                                        const objc_class_disk_t *disk) {
    if (!disk) return NULL;
    /* Read class_ro_t via the data pointer.                          */
    const class_ro_t *ro = (const class_ro_t *)
        ((const uint8_t *)macho_loaded_base(img) +
         (disk->data & 0x00ffffffu)); /* masked reloc offset */
    if (!ro) return NULL;

    const char *name = read_cstring(img, ro->name);
    if (!name) return NULL;

    objc_class_t *cls = (objc_class_t *)calloc(1, sizeof *cls);
    if (!cls) return NULL;
    cls->name          = strdup(name);
    cls->instance_size = ro->instanceSize;

    /* Walk ivars.                                                    */
    if (ro->ivars) {
        const struct {
            uint32_t count;
            objc_ivar_disk_t ivars[1];
        } *ivs = (const void *)((const uint8_t *)macho_loaded_base(img)
                                + (ro->ivars & 0x00ffffffu));
        if (ivs->count > 0 && ivs->count < 1024) {
            cls->ivars = (objc_ivar_t *)calloc(ivs->count, sizeof(objc_ivar_t));
            for (uint32_t i = 0; i < ivs->count; i++) {
                cls->ivars[i].name   = read_cstring(img, ivs->ivars[i].name);
                cls->ivars[i].types  = read_cstring(img, ivs->ivars[i].types);
                cls->ivars[i].offset = ivs->ivars[i].offset;
            }
            cls->nivars = ivs->count;
        }
    }

    /* Walk methods.                                                  */
    if (ro->baseMethods) {
        const struct {
            uint32_t count;
            objc_method_disk_t methods[1];
        } *ml = (const void *)((const uint8_t *)macho_loaded_base(img)
                               + (ro->baseMethods & 0x00ffffffu));
        if (ml->count > 0 && ml->count < 4096) {
            extern afros_status_t ObjcAddMethod(objc_class_t *,
                                                const char *,
                                                void (*)(void *, void *,
                                                         void *, void *),
                                                const char *);
            /* IMPs are rebased lazily; we record the disk pointer.   */
            for (uint32_t i = 0; i < ml->count; i++) {
                const char *mname = read_cstring(img, ml->methods[i].name);
                const char *mtypes = read_cstring(img, ml->methods[i].types);
                if (mname) {
                    ObjcAddMethod(cls, mname, NULL, mtypes);
                }
            }
        }
    }

    ObjcRegisterClass(cls);
    return cls;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t ClassLoadFromImage(macho_image_t *img) {
    if (!img) return AFROS_ERROR_INVALID_PARAM;
    section_64_t *sect = NULL;
    if (MachoGetSections(img, "__DATA", "__objc_classlist",
                         &sect) != AFROS_SUCCESS) {
        /* Try __DATA_DIRTY for newer images.                          */
        if (MachoGetSections(img, "__DATA_DIRTY", "__objc_classlist",
                             &sect) != AFROS_SUCCESS) {
            return AFROS_SUCCESS; /* no classes — nothing to do */
        }
    }
    if (sect->size == 0 || sect->addr == 0) return AFROS_SUCCESS;

    void *base = macho_loaded_base(img);
    if (!base) return AFROS_ERROR;

    uint64_t *ptrs = (uint64_t *)((uint8_t *)base +
                                  (sect->addr & 0x00ffffffu));
    uint32_t count = (uint32_t)(sect->size / sizeof(uint64_t));
    if (count > 65536) return AFROS_ERROR;

    for (uint32_t i = 0; i < count; i++) {
        const objc_class_disk_t *disk = (const objc_class_disk_t *)
            ((uint8_t *)base + (ptrs[i] & 0x00ffffffu));
        register_from_disk(img, disk);
    }
    return AFROS_SUCCESS;
}

afros_status_t ClassLoadAll(macho_image_t *img) {
    /* Load categories first, then classes, then merge.               */
    afros_status_t s = ClassLoadFromImage(img);
    if (s != AFROS_SUCCESS) return s;

    section_64_t *catlist = NULL;
    if (MachoGetSections(img, "__DATA", "__objc_catlist",
                         &catlist) != AFROS_SUCCESS) {
        return AFROS_SUCCESS;
    }
    void *base = macho_loaded_base(img);
    if (!base || catlist->size == 0) return AFROS_SUCCESS;
    uint64_t *ptrs = (uint64_t *)((uint8_t *)base +
                                  (catlist->addr & 0x00ffffffu));
    uint32_t count = (uint32_t)(catlist->size / sizeof(uint64_t));
    for (uint32_t i = 0; i < count; i++) {
        /* Categories share the on-disk layout with classes.          */
        const objc_class_disk_t *cat = (const objc_class_disk_t *)
            ((uint8_t *)base + (ptrs[i] & 0x00ffffffu));
        const class_ro_t *ro = (const class_ro_t *)
            ((uint8_t *)base + (cat->data & 0x00ffffffu));
        if (!ro) continue;
        const char *name = read_cstring(img, ro->name);
        objc_class_t *host = name ? ObjcGetClass(name) : NULL;
        if (host) {
            objc_class_t *category = (objc_class_t *)calloc(1, sizeof *category);
            category->name = "category";
            ObjcAddCategory(host, category);
        }
    }
    return AFROS_SUCCESS;
}

/* Resolve a class pointer recorded in disk form to a runtime class.   */
objc_class_t *ClassLookupByDiskPtr(macho_image_t *img, uint64_t ptr) {
    if (!img) return NULL;
    const objc_class_disk_t *disk = (const objc_class_disk_t *)
        ((const uint8_t *)macho_loaded_base(img) + (ptr & 0x00ffffffu));
    if (!disk) return NULL;
    const class_ro_t *ro = (const class_ro_t *)
        ((const uint8_t *)macho_loaded_base(img) + (disk->data & 0x00ffffffu));
    if (!ro) return NULL;
    const char *name = read_cstring(img, ro->name);
    return name ? ObjcGetClass(name) : NULL;
}
