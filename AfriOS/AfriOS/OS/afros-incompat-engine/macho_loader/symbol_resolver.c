/**
 * @file symbol_resolver.c
 * @brief Resolve exported and imported symbols of a Mach-O image.
 *
 * Combines the symbol table (LC_SYMTAB), the indirect symbol table
 * (LC_DYSYMTAB) and the bind opcodes to look up symbols either locally
 * or via the registered dyld resolver callback.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Accessors for the symbol/string tables                              */
/* ------------------------------------------------------------------ */

static const nlist_64_t *symtab_entries(const macho_image_t *img,
                                        uint32_t *count) {
    const symtab_command_t *st = macho_image_symtab(img);
    if (!st || st->nsyms == 0) {
        if (count) *count = 0;
        return NULL;
    }
    if (count) *count = st->nsyms;
    return (const nlist_64_t *)((const uint8_t *)macho_image_base(img)
                                + st->symoff);
}

static const char *symtab_strings(const macho_image_t *img,
                                  uint32_t *size_out) {
    const symtab_command_t *st = macho_image_symtab(img);
    if (!st) { if (size_out) *size_out = 0; return NULL; }
    if (size_out) *size_out = st->strsize;
    return (const char *)macho_image_base(img) + st->stroff;
}

static const uint32_t *indirect_symbols(const macho_image_t *img,
                                        uint32_t *count) {
    const dysymtab_command_t *dys = macho_image_dysymtab(img);
    if (!dys || dys->nindirectsyms == 0) {
        if (count) *count = 0;
        return NULL;
    }
    if (count) *count = dys->nindirectsyms;
    return (const uint32_t *)((const uint8_t *)macho_image_base(img)
                              + dys->indirectsymoff);
}

/* ------------------------------------------------------------------ */
/* Exported symbol lookup                                              */
/* ------------------------------------------------------------------ */

afros_status_t SymbolLookupExport(macho_image_t *img, const char *name,
                                  nlist_64_t **out) {
    if (!img || !name || !out) return AFROS_ERROR_INVALID_PARAM;
    *out = NULL;

    const dysymtab_command_t *dys = macho_image_dysymtab(img);
    if (!dys) return AFROS_ERROR;

    uint32_t nsyms = 0;
    const nlist_64_t *syms = symtab_entries(img, &nsyms);
    uint32_t strsize = 0;
    const char *strs = symtab_strings(img, &strsize);
    if (!syms || !strs) return AFROS_ERROR;

    /* Only exported (externally visible) symbols.                    */
    uint32_t begin = dys->iextdefsym;
    uint32_t end   = begin + dys->nextdefsym;
    if (begin >= nsyms || end > nsyms) return AFROS_ERROR;

    for (uint32_t i = begin; i < end; i++) {
        uint32_t strx = syms[i].n_strx;
        if (strx >= strsize) continue;
        const char *sym = strs + strx;
        /* Skip the leading underscore that Mach-O emits for c symbols */
        if (sym[0] == '_') sym++;
        if (strcmp(sym, name) == 0) {
            *out = (nlist_64_t *)&syms[i];
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Full resolve: try export, then ask the dyld resolver                */
/* ------------------------------------------------------------------ */

static void *g_dyld_resolver_ctx = NULL;
static symbol_resolver_cb_t g_dyld_resolver = NULL;

void SymbolSetDyldResolver(symbol_resolver_cb_t cb, void *ctx) {
    g_dyld_resolver = cb;
    g_dyld_resolver_ctx = ctx;
}

afros_status_t SymbolResolve(macho_image_t *img, const char *name,
                             void **out) {
    if (!name || !out) return AFROS_ERROR_INVALID_PARAM;
    *out = NULL;

    /* 1. Try local exports of the image.                            */
    nlist_64_t *nl = NULL;
    if (img && SymbolLookupExport(img, name, &nl) == AFROS_SUCCESS) {
        /* Add the runtime slide so callers get a usable address.     */
        uint64_t slide = macho_loaded_slide(img);
        *out = (void *)(uintptr_t)(nl->n_value + slide);
        return AFROS_SUCCESS;
    }

    /* 2. Fall back to the registered dyld resolver.                  */
    if (g_dyld_resolver) {
        void *p = g_dyld_resolver(name, g_dyld_resolver_ctx);
        if (p) {
            *out = p;
            return AFROS_SUCCESS;
        }
    }

    /* 3. Last resort: well-known symbols that we always provide.     */
    if (strcmp(name, "_dyld_stub_binder") == 0) {
        *out = (void *)0xdeadbeef; /* bound lazily by BindLazyAt()    */
        return AFROS_SUCCESS;
    }
    return AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Indirect symbol table lookup                                        */
/* ------------------------------------------------------------------ */

afros_status_t SymbolIndirectAt(macho_image_t *img, uint32_t index,
                                const char **name_out) {
    if (!img || !name_out) return AFROS_ERROR_INVALID_PARAM;
    *name_out = NULL;
    uint32_t nind = 0;
    const uint32_t *ind = indirect_symbols(img, &nind);
    if (!ind || index >= nind) return AFROS_ERROR_INVALID_PARAM;

    uint32_t strx = ind[index];
    uint32_t strsize = 0;
    const char *strs = symtab_strings(img, &strsize);
    if (!strs || strx >= strsize) return AFROS_ERROR;

    *name_out = strs + strx;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Enumerate all exports (used by class loader to discover classes)    */
/* ------------------------------------------------------------------ */

afros_status_t SymbolForEachExport(macho_image_t *img,
                                   void (*cb)(const char *name,
                                              uint64_t value,
                                              void *ctx),
                                   void *ctx) {
    if (!img || !cb) return AFROS_ERROR_INVALID_PARAM;
    const dysymtab_command_t *dys = macho_image_dysymtab(img);
    if (!dys) return AFROS_ERROR;
    uint32_t nsyms = 0;
    const nlist_64_t *syms = symtab_entries(img, &nsyms);
    uint32_t strsize = 0;
    const char *strs = symtab_strings(img, &strsize);
    if (!syms || !strs) return AFROS_ERROR;

    uint64_t slide = macho_loaded_slide(img);
    uint32_t begin = dys->iextdefsym;
    uint32_t end   = begin + dys->nextdefsym;
    if (begin >= nsyms || end > nsyms) return AFROS_ERROR;
    for (uint32_t i = begin; i < end; i++) {
        uint32_t strx = syms[i].n_strx;
        if (strx >= strsize) continue;
        cb(strs + strx, syms[i].n_value + slide, ctx);
    }
    return AFROS_SUCCESS;
}

void SymbolClearDyldResolver(void) {
    g_dyld_resolver = NULL;
    g_dyld_resolver_ctx = NULL;
}
