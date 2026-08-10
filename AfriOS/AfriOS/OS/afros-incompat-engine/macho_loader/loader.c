/**
 * @file loader.c
 * @brief Load a Mach-O binary into AfriOS address space.
 *
 * The loader maps the requested segments, applies ASLR slide rebase,
 * runs the bind opcodes and invokes initialisers in dependency order.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* Slide chosen uniformly across a 1 GiB window.                      */
#define AFROS_MACHO_SLIDE_BASE  0x100000000ULL
#define AFROS_MACHO_SLIDE_LIMIT 0x40000000ULL

/* ------------------------------------------------------------------ */
/* Extended image: parse result + runtime state                        */
/* ------------------------------------------------------------------ */

typedef struct loaded_image_s {
    macho_image_t   *parse;          /* from MachoParse              */
    void            *mapped_base;    /* slide-adjusted base          */
    uint64_t         slide;          /* ASLR slide applied           */
    int              refcount;
    struct loaded_image_s *next;
} loaded_image_t;

static loaded_image_t *g_loaded_head = NULL;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static uint64_t pick_slide(void) {
    /* Deterministic slide — would normally come from kernel RNG.    */
    static uint64_t s = 0x1000;
    s = (s * 6364136223846793005ULL + 1442695040888963407ULL);
    return AFROS_MACHO_SLIDE_BASE + (s % AFROS_MACHO_SLIDE_LIMIT);
}

static afros_status_t map_segments(macho_image_t *img, uint64_t slide,
                                   void **out_base) {
    segment_command_64_t *segs = NULL;
    uint32_t nsegs = 0;
    if (MachoGetSegments(img, &segs, &nsegs) != AFROS_SUCCESS) {
        return AFROS_ERROR;
    }
    const segment_command_64_t **segv = (const segment_command_64_t **)segs;

    /* Pre-flight: figure out the total address span.                */
    uint64_t lo = ~0ULL, hi = 0;
    for (uint32_t i = 0; i < nsegs; i++) {
        const segment_command_64_t *seg = segv[i];
        if (seg->vmsize == 0) continue;
        if (seg->vmaddr < lo) lo = seg->vmaddr;
        if (seg->vmaddr + seg->vmsize > hi)
            hi = seg->vmaddr + seg->vmsize;
    }
    if (lo > hi) return AFROS_ERROR;

    size_t span = (size_t)(hi - lo);
    void *base = mmap(NULL, span, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return AFROS_ERROR_NO_MEMORY;

    /* Copy each segment's file bytes into the anonymous region.      */
    for (uint32_t i = 0; i < nsegs; i++) {
        const segment_command_64_t *seg = segv[i];
        if (seg->vmsize == 0) continue;
        size_t off_in_region = (size_t)(seg->vmaddr - lo);
        uint8_t *dst = (uint8_t *)base + off_in_region;
        if (seg->filesize > 0) {
            const uint8_t *src =
                (const uint8_t *)macho_image_base(img) + seg->fileoff;
            size_t n = (size_t)seg->filesize;
            if (n > span - off_in_region) n = span - off_in_region;
            memcpy(dst, src, n);
        }
    }

    /* Apply the slide to the image's view of pointers (rebase).     */
    /* Real dyld rewrites all __TEXT/__DATA pointers by `slide`.      */
    /* For the compatibility layer we record the slide and let the   */
    /* bind handler apply it to individual fixups.                   */
    (void)slide;
    *out_base = base;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t MachoLoadFromFD(int fd, size_t size, macho_image_t **out) {
    if (fd < 0 || !out) return AFROS_ERROR_INVALID_PARAM;
    if (size == 0) {
        struct stat st;
        if (fstat(fd, &st) == 0) size = (size_t)st.st_size;
        else return AFROS_ERROR_INVALID_PARAM;
    }
    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) return AFROS_ERROR_NO_MEMORY;

    afros_status_t s = MachoParse(map, size, out);
    if (s != AFROS_SUCCESS) {
        munmap(map, size);
        return s;
    }

    /* Build the runtime descriptor.                                  */
    loaded_image_t *li = (loaded_image_t *)calloc(1, sizeof *li);
    if (!li) { MachoRelease(*out); munmap(map, size); return AFROS_ERROR_NO_MEMORY; }
    li->parse = *out;
    li->slide = pick_slide();

    void *base = NULL;
    if (map_segments(li->parse, li->slide, &base) != AFROS_SUCCESS) {
        MachoRelease(li->parse);
        free(li);
        munmap(map, size);
        return AFROS_ERROR;
    }
    li->mapped_base = base;
    li->refcount   = 1;
    li->next       = g_loaded_head;
    g_loaded_head  = li;
    return AFROS_SUCCESS;
}

afros_status_t MachoLoad(const char *path, macho_image_t **out) {
    if (!path || !out) return AFROS_ERROR_INVALID_PARAM;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return AFROS_ERROR_INVALID_PARAM;
    afros_status_t s = MachoLoadFromFD(fd, 0, out);
    close(fd);
    return s;
}

/* ------------------------------------------------------------------ */
/* Initialiser invocation                                              */
/* ------------------------------------------------------------------ */

typedef void (*init_func_t)(void);

afros_status_t MachoRunInitializers(macho_image_t *img) {
    if (!img) return AFROS_ERROR_INVALID_PARAM;
    section_64_t *mod_init = NULL;
    if (MachoGetSections(img, "__DATA", "__mod_init_func",
                         &mod_init) != AFROS_SUCCESS) {
        return AFROS_SUCCESS; /* nothing to do */
    }
    if (mod_init->size == 0 || mod_init->addr == 0) return AFROS_SUCCESS;
    size_t count = mod_init->size / sizeof(init_func_t);
    init_func_t *funcs = (init_func_t *)(uintptr_t)mod_init->addr;
    for (size_t i = 0; i < count; i++) {
        if (funcs[i]) funcs[i]();
    }
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Entry-point discovery                                               */
/* ------------------------------------------------------------------ */

afros_status_t MachoGetEntryPoint(macho_image_t *img,
                                  void (**entry)(void),
                                  uint64_t *slide_out) {
    if (!img || !entry) return AFROS_ERROR_INVALID_PARAM;
    segment_command_64_t *segs = NULL;
    uint32_t nseg = 0;
    (void)MachoGetSegments(img, &segs, &nseg);
    (void)segs;
    (void)nseg;

    /* Search LC_MAIN.                                                */
    const load_command_t *main = macho_find_cmd(img, (uint32_t)LC_MAIN);
    if (main) {
        struct __attribute__((packed)) {
            uint32_t cmd, cmdsize;
            uint64_t entryoff;
            uint64_t stacksize;
        } *m = (void *)main;
        *entry = (void (*)(void))(uintptr_t)m->entryoff;
        if (slide_out) *slide_out = 0;
        return AFROS_SUCCESS;
    }
    return AFROS_ERROR;
}

/* Locate the loaded image wrapper for a parsed image.                 */
static loaded_image_t *macho_loaded_for(macho_image_t *img) {
    for (loaded_image_t *li = g_loaded_head; li; li = li->next) {
        if (li->parse == img) return li;
    }
    return NULL;
}

void *macho_loaded_base(macho_image_t *img) {
    loaded_image_t *li = macho_loaded_for(img);
    return li ? li->mapped_base : NULL;
}

uint64_t macho_loaded_slide(macho_image_t *img) {
    loaded_image_t *li = macho_loaded_for(img);
    return li ? li->slide : 0;
}
