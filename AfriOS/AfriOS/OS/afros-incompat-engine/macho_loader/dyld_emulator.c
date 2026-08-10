/**
 * @file dyld_emulator.c
 * @brief Emulates the iOS/macOS dynamic linker (dyld).
 *
 * Maintains a registry of loaded images, exposes dlopen/dlsym/dlclose
 * and provides the dyld_stub_binder trampoline used by the binding
 * handler to lazily resolve undefined symbols.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define AFROS_DYLD_MAX_IMAGES 64

/* dlopen mode flags (subset of the Apple constants).                 */
#define AFROS_RTLD_LAZY     0x1
#define AFROS_RTLD_NOW      0x2
#define AFROS_RTLD_GLOBAL   0x100
#define AFROS_RTLD_LOCAL    0x200

/* ------------------------------------------------------------------ */
/* Image registry                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char            path[512];
    macho_image_t  *image;
    int             refcount;
    bool            used;
} dyld_entry_t;

static dyld_entry_t g_dyld_images[AFROS_DYLD_MAX_IMAGES];
static bool         g_dyld_inited = false;

/* Forward declaration.                                                */
void *dyld_internal_resolver(const char *name, void *ctx);

/* ------------------------------------------------------------------ */
/* Init / shutdown                                                     */
/* ------------------------------------------------------------------ */

afros_status_t DyldInit(void) {
    if (g_dyld_inited) return AFROS_SUCCESS;
    memset(g_dyld_images, 0, sizeof g_dyld_images);

    /* Register a built-in resolver that searches every loaded image. */
    SymbolSetDyldResolver(dyld_internal_resolver, NULL);
    g_dyld_inited = true;
    return AFROS_SUCCESS;
}

void DyldShutdown(void) {
    for (int i = 0; i < AFROS_DYLD_MAX_IMAGES; i++) {
        if (g_dyld_images[i].used && g_dyld_images[i].image) {
            MachoRelease(g_dyld_images[i].image);
        }
    }
    memset(g_dyld_images, 0, sizeof g_dyld_images);
    SymbolClearDyldResolver();
    g_dyld_inited = false;
}

/* ------------------------------------------------------------------ */
/* Resolver callback                                                   */
/* ------------------------------------------------------------------ */

void *dyld_internal_resolver(const char *name, void *ctx) {
    (void)ctx;
    if (!name) return NULL;
    if (name[0] == '_') name++;
    /* Search every loaded image for an exported symbol.              */
    for (int i = 0; i < AFROS_DYLD_MAX_IMAGES; i++) {
        if (!g_dyld_images[i].used || !g_dyld_images[i].image) continue;
        void *p = NULL;
        if (SymbolResolve(g_dyld_images[i].image, name, &p) == AFROS_SUCCESS) {
            return p;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* dlopen / dlsym / dlclose                                            */
/* ------------------------------------------------------------------ */

static int find_free_slot(void) {
    for (int i = 0; i < AFROS_DYLD_MAX_IMAGES; i++) {
        if (!g_dyld_images[i].used) return i;
    }
    return -1;
}

static int find_by_path(const char *path) {
    for (int i = 0; i < AFROS_DYLD_MAX_IMAGES; i++) {
        if (g_dyld_images[i].used &&
            strcmp(g_dyld_images[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

void *DyldDlopen(const char *path, int mode) {
    (void)mode;
    if (!g_dyld_inited) DyldInit();
    if (!path) return NULL;

    int slot = find_by_path(path);
    if (slot >= 0) {
        g_dyld_images[slot].refcount++;
        return g_dyld_images[slot].image;
    }

    slot = find_free_slot();
    if (slot < 0) return NULL;

    macho_image_t *img = NULL;
    if (MachoLoad(path, &img) != AFROS_SUCCESS) return NULL;

    /* Bind all imports immediately so the image is ready to use.     */
    BindProcessAll(img, dyld_internal_resolver, NULL);
    MachoRunInitializers(img);

    strncpy(g_dyld_images[slot].path, path,
            sizeof g_dyld_images[slot].path - 1);
    g_dyld_images[slot].image   = img;
    g_dyld_images[slot].refcount = 1;
    g_dyld_images[slot].used    = true;
    return img;
}

void *DyldDlsym(void *handle, const char *symbol) {
    if (!symbol) return NULL;
    if (!g_dyld_inited) DyldInit();
    const char *sym = symbol;
    if (sym[0] == '_') sym++;

    /* NULL handle means "search all images".                          */
    if (handle == NULL) {
        return dyld_internal_resolver(sym, NULL);
    }

    macho_image_t *img = (macho_image_t *)handle;
    void *p = NULL;
    if (SymbolResolve(img, sym, &p) == AFROS_SUCCESS) return p;
    /* Fall back to the global search.                                 */
    return dyld_internal_resolver(sym, NULL);
}

int DyldDlclose(void *handle) {
    if (!handle) return -1;
    for (int i = 0; i < AFROS_DYLD_MAX_IMAGES; i++) {
        if (g_dyld_images[i].used && g_dyld_images[i].image == handle) {
            if (--g_dyld_images[i].refcount <= 0) {
                MachoRelease(g_dyld_images[i].image);
                g_dyld_images[i].used  = false;
                g_dyld_images[i].image = NULL;
            }
            return 0;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* dyld_stub_binder trampoline                                         */
/* ------------------------------------------------------------------ */

typedef void *(*stub_binder_t)(macho_image_t *img, void **slot);

/* Real mach-o stubs call dyld_stub_binder(image, slot, selector).    */
/* We expose a C-callable trampoline that returns the resolved addr.  */
void *dyld_stub_binder(macho_image_t *img, void **slot) {
    if (BindLazyAt(img, slot) == AFROS_SUCCESS) {
        return *slot;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Shared-cache lookup stub                                            */
/* ------------------------------------------------------------------ */

afros_status_t DyldSharedCacheLookup(const char *name, void **out) {
    if (!name || !out) return AFROS_ERROR_INVALID_PARAM;
    *out = NULL;
    /* The iOS shared cache is a single mmaped file containing the    */
    /* system frameworks. AfriOS does not ship a real shared cache,   */
    /* so we fall back to per-image lookup.                            */
    void *p = dyld_internal_resolver(name, NULL);
    if (p) { *out = p; return AFROS_SUCCESS; }
    return AFROS_ERROR;
}

afros_status_t DyldEnumerateImages(void (*cb)(const char *path,
                                              macho_image_t *img,
                                              void *ctx),
                                   void *ctx) {
    if (!cb) return AFROS_ERROR_INVALID_PARAM;
    for (int i = 0; i < AFROS_DYLD_MAX_IMAGES; i++) {
        if (g_dyld_images[i].used) {
            cb(g_dyld_images[i].path, g_dyld_images[i].image, ctx);
        }
    }
    return AFROS_SUCCESS;
}
