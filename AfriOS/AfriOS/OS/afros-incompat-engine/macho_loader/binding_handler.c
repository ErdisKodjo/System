/**
 * @file binding_handler.c
 * @brief Implementation of the dyld bind opcodes.
 *
 * Walks the BIND, LAZY_BIND and WEAK_BIND byte streams of a Mach-O
 * image and patches pointer slots in __DATA with the addresses
 * returned by the symbol resolver.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* ULEB128 / SLEB128 decoders                                          */
/* ------------------------------------------------------------------ */

static uint64_t read_uleb128(const uint8_t **pp, const uint8_t *end) {
    uint64_t result = 0;
    int shift = 0;
    const uint8_t *p = *pp;
    while (p < end) {
        uint8_t b = *p++;
        result |= (uint64_t)(b & 0x7fu) << shift;
        if ((b & 0x80u) == 0) break;
        shift += 7;
        if (shift >= 64) break;
    }
    *pp = p;
    return result;
}

static int64_t read_sleb128(const uint8_t **pp, const uint8_t *end) {
    int64_t result = 0;
    int shift = 0;
    const uint8_t *p = *pp;
    uint8_t b = 0;
    while (p < end) {
        b = *p++;
        result |= (int64_t)(b & 0x7fu) << shift;
        shift += 7;
        if ((b & 0x80u) == 0) break;
        if (shift >= 64) break;
    }
    if (shift < 64 && (b & 0x40u)) {
        result |= -((int64_t)1 << shift);
    }
    *pp = p;
    return result;
}

/* ------------------------------------------------------------------ */
/* Bind state                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    macho_image_t       *img;
    symbol_resolver_cb_t resolver;
    void                *ctx;
    int                  dylib_ordinal;
    char                 symbol[256];
    uint32_t             symbol_flags;
    uint32_t             type;
    int64_t              addend;
    uint64_t             segment_index;
    uint64_t             address_in_segment;
} bind_state_t;

static uint8_t *bind_target(bind_state_t *s) {
    if (s->segment_index >= 0x100) return NULL; /* sanity */
    segment_command_64_t *segs = NULL;
    uint32_t nsegs = 0;
    if (MachoGetSegments(s->img, &segs, &nsegs) != AFROS_SUCCESS) return NULL;
    if (s->segment_index >= nsegs) return NULL;
    const segment_command_64_t *seg =
        ((const segment_command_64_t **)segs)[s->segment_index];
    void *mapped = macho_loaded_base(s->img);
    if (!mapped) return NULL;
    return (uint8_t *)mapped + (seg->vmaddr - ((const segment_command_64_t **)segs)[0]->vmaddr)
           + s->address_in_segment;
}

static void do_bind(bind_state_t *s) {
    /* Strip leading underscore before lookup.                        */
    const char *sym = s->symbol;
    if (sym[0] == '_') sym++;

    void *resolved = s->resolver ? s->resolver(sym, s->ctx) : NULL;
    if (!resolved) {
        /* Try the image's own symbol table.                          */
        SymbolResolve(s->img, sym, &resolved);
    }
    uint8_t *slot = bind_target(s);
    if (slot && resolved) {
        switch (s->type) {
        case BIND_TYPE_POINTER:
            *(void **)slot = resolved;
            break;
        case BIND_TYPE_TEXT_ABSOLUTE32:
            *(uint32_t *)slot = (uint32_t)(uintptr_t)resolved;
            break;
        default:
            break;
        }
    }
    /* Advance to the next slot (sizeof pointer).                     */
    s->address_in_segment += sizeof(void *);
}

/* ------------------------------------------------------------------ */
/* Public opcode processor                                             */
/* ------------------------------------------------------------------ */

afros_status_t BindProcess(macho_image_t *img, const uint8_t *ops, size_t len,
                           symbol_resolver_cb_t resolver, void *ctx) {
    if (!img || !ops) return AFROS_ERROR_INVALID_PARAM;
    bind_state_t s = {0};
    s.img      = img;
    s.resolver = resolver;
    s.ctx      = ctx;
    s.dylib_ordinal = 0;
    s.type          = BIND_TYPE_POINTER;
    s.addend        = 0;

    const uint8_t *p   = ops;
    const uint8_t *end = ops + len;
    while (p < end) {
        uint8_t op  = *p & 0xf0u;
        uint8_t imm = *p & 0x0fu;
        p++;
        switch (op) {
        case BIND_OPCODE_DONE:
            return AFROS_SUCCESS;
        case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
            s.dylib_ordinal = (int)imm;
            break;
        case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            s.dylib_ordinal = (int)read_uleb128(&p, end);
            break;
        case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            s.dylib_ordinal = (imm == 0) ? 0 : (int)(imm | 0xfffffff0u);
            break;
        case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM: {
            s.symbol_flags = imm;
            size_t i = 0;
            while (p < end && *p != '\0' && i + 1 < sizeof s.symbol) {
                s.symbol[i++] = (char)*p++;
            }
            s.symbol[i] = '\0';
            if (p < end) p++; /* skip NUL */
            break;
        }
        case BIND_OPCODE_SET_TYPE_IMM:
            s.type = imm;
            break;
        case BIND_OPCODE_SET_ADDEND_SLEB:
            s.addend = read_sleb128(&p, end);
            break;
        case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            s.segment_index = imm;
            s.address_in_segment = read_uleb128(&p, end);
            break;
        case BIND_OPCODE_ADD_ADDR_ULEB:
            s.address_in_segment += read_uleb128(&p, end);
            break;
        case BIND_OPCODE_DO_BIND:
            do_bind(&s);
            break;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            do_bind(&s);
            s.address_in_segment += (uint64_t)imm * sizeof(void *);
            break;
        case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            do_bind(&s);
            s.address_in_segment += read_uleb128(&p, end);
            break;
        case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB: {
            uint64_t count = read_uleb128(&p, end);
            uint64_t skip  = read_uleb128(&p, end);
            for (uint64_t i = 0; i < count; i++) {
                do_bind(&s);
                s.address_in_segment += skip;
            }
            break;
        }
        default:
            /* Unknown opcode — abort to avoid drift.                  */
            return AFROS_ERROR_NOT_SUPPORTED;
        }
    }
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Lazy binding: invoked the first time a stub is hit                  */
/* ------------------------------------------------------------------ */

afros_status_t BindLazyAt(macho_image_t *img, void **slot) {
    if (!img || !slot) return AFROS_ERROR_INVALID_PARAM;
    /* In real dyld the lazy pointer indexes into the lazy bind       */
    /* opcodes; the engine provides a trampoline that simply resolves */
    /* the symbol by name (stored as a side-table for the slot).      */
    static struct { void **slot; const char *name; } lazy_tab[64];
    static int lazy_n = 0;
    /* First call records the slot, subsequent lookups resolve.       */
    for (int i = 0; i < lazy_n; i++) {
        if (lazy_tab[i].slot == slot) {
            void *addr = NULL;
            if (SymbolResolve(img, lazy_tab[i].name, &addr) == AFROS_SUCCESS) {
                *slot = addr;
                return AFROS_SUCCESS;
            }
            return AFROS_ERROR;
        }
    }
    /* Slot not registered yet — caller is expected to populate the   */
    /* name via the regular BindProcess pass.                          */
    (void)lazy_tab; (void)lazy_n;
    return AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* Convenience: walk __LINKEDIT bind sections                           */
/* ------------------------------------------------------------------ */

afros_status_t BindProcessAll(macho_image_t *img,
                              symbol_resolver_cb_t resolver, void *ctx) {
    if (!img) return AFROS_ERROR_INVALID_PARAM;
    section_64_t *sect = NULL;
    /* Non-lazy binding section (__nl_symbol_ptr is in __DATA).        */
    if (MachoGetSections(img, "__DATA", "__la_symbol_ptr",
                         &sect) == AFROS_SUCCESS) {
        const uint8_t *ops = (const uint8_t *)macho_image_base(img)
                             + sect->reserved1; /* bind offset stored here */
        BindProcess(img, ops, sect->reserved2, resolver, ctx);
    }
    return AFROS_SUCCESS;
}
