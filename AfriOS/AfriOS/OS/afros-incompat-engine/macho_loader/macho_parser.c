/**
 * @file macho_parser.c
 * @brief Mach-O binary header & load-command parser for the AfriOS
 *        Apple compatibility layer.
 *
 * Parses 32-bit, 64-bit and Universal (fat) Mach-O binaries and exposes
 * a small in-memory image descriptor used by the rest of the loader.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Internal image structure                                            */
/* ------------------------------------------------------------------ */

struct macho_image_s {
    uint32_t               magic;
    uint32_t               cputype;
    uint32_t               filetype;
    uint32_t               flags;
    uint32_t               ncmds;
    const load_command_t **cmds;            /* array of pointers   */
    const segment_command_64_t **segments;  /* 64-bit normalised   */
    uint32_t               nsegments;
    const symtab_command_t  *symtab;
    const dysymtab_command_t *dysymtab;
    const linkedit_data_command_t *codesig;
    const void            *base;            /* original buffer     */
    size_t                 size;
    bool                   swap;            /* endian swap needed  */
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static inline uint32_t swap32(uint32_t v) {
    return ((v & 0xffu) << 24) | ((v & 0xff00u) << 8) |
           ((v >> 8) & 0xff00u) | ((v >> 24) & 0xffu);
}

static bool macho_need_swap(uint32_t magic) {
    /* On a little-endian host, big-endian Mach-O needs swapping.   */
    return (magic == MH_CIGAM || magic == MH_CIGAM_64 ||
            magic == FAT_CIGAM || magic == FAT_CIGAM_64);
}

static uint32_t macho_read32(const void *p, bool swap) {
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return swap ? swap32(v) : v;
}

static const uint8_t *macho_pick_arch(const uint8_t *data, size_t size) {
    /* Universal/fat binary support: pick ARM64 slice, else first.   */
    if (size < sizeof(uint32_t)) return NULL;
    uint32_t magic;
    memcpy(&magic, data, sizeof magic);
    if (magic == FAT_MAGIC || magic == FAT_CIGAM ||
        magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
        /* Read first arch entry (simplified).                       */
        if (size < 8) return NULL;
        uint32_t nfat = macho_read32(data + 4, magic == FAT_CIGAM);
        if (nfat == 0) return NULL;
        if (size < 20) return NULL;
        uint32_t offset = macho_read32(data + 8, magic == FAT_CIGAM);
        if (offset >= size) return NULL;
        return data + offset;
    }
    return data;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t MachoParse(const void *bytes, size_t size,
                          macho_image_t **out) {
    if (!bytes || !out || size < sizeof(uint32_t)) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    const uint8_t *data = (const uint8_t *)bytes;
    const uint8_t *base = macho_pick_arch(data, size);
    if (!base) return AFROS_ERROR_INVALID_PARAM;

    size_t remaining = size - (size_t)(base - data);
    if (remaining < sizeof(mach_header_t)) return AFROS_ERROR;

    macho_image_t *img = (macho_image_t *)calloc(1, sizeof *img);
    if (!img) return AFROS_ERROR_NO_MEMORY;

    img->base = base;
    img->size = remaining;

    const mach_header_t *hdr = (const mach_header_t *)base;
    img->magic = macho_read32(&hdr->magic, false);
    img->swap  = macho_need_swap(img->magic);

    /* Normalise to 64-bit semantics where possible.                 */
    uint32_t m = img->magic;
    if (m == MH_CIGAM || m == MH_CIGAM_64) m = swap32(m);

    if (m != MH_MAGIC && m != MH_MAGIC_64) {
        free(img);
        return AFROS_ERROR_NOT_SUPPORTED;
    }
    img->cputype  = macho_read32(&hdr->cputype, img->swap);
    img->filetype = macho_read32(&hdr->filetype, img->swap);
    img->flags    = macho_read32(&hdr->flags, img->swap);
    img->ncmds    = macho_read32(&hdr->ncmds, img->swap);

    if (img->ncmds == 0 || img->ncmds > 4096) {
        free(img);
        return AFROS_ERROR;
    }

    img->cmds = (const load_command_t **)calloc(img->ncmds,
                                                sizeof(load_command_t *));
    if (!img->cmds) { free(img); return AFROS_ERROR_NO_MEMORY; }

    /* Walk the load commands.                                        */
    size_t hdr_size = (m == MH_MAGIC_64) ? 32 : 28;
    const uint8_t *p   = base + hdr_size;
    const uint8_t *end = base + remaining;
    uint32_t i;
    for (i = 0; i < img->ncmds && p + sizeof(load_command_t) <= end; i++) {
        const load_command_t *lc = (const load_command_t *)p;
        uint32_t cmd = macho_read32(&lc->cmd, img->swap);
        uint32_t sz  = macho_read32(&lc->cmdsize, img->swap);
        if (sz == 0 || p + sz > end) break;
        img->cmds[i] = lc;

        switch (cmd) {
        case LC_SEGMENT_64:
            img->nsegments++;
            break;
        case LC_SYMTAB:
            img->symtab = (const symtab_command_t *)lc;
            break;
        case LC_DYSYMTAB:
            img->dysymtab = (const dysymtab_command_t *)lc;
            break;
        case LC_CODE_SIGNATURE:
            img->codesig = (const linkedit_data_command_t *)lc;
            break;
        default:
            break;
        }
        p += sz;
    }
    img->ncmds = i;

    if (img->nsegments > 0) {
        img->segments = (const segment_command_64_t **)calloc(
            img->nsegments, sizeof(segment_command_64_t *));
        uint32_t s = 0;
        for (uint32_t j = 0; j < img->ncmds; j++) {
            const load_command_t *lc = img->cmds[j];
            if (macho_read32(&lc->cmd, img->swap) == LC_SEGMENT_64) {
                img->segments[s++] = (const segment_command_64_t *)lc;
            }
        }
    }

    *out = img;
    return AFROS_SUCCESS;
}

afros_status_t MachoGetSegments(macho_image_t *img,
                                segment_command_64_t **out,
                                uint32_t *count) {
    if (!img || !out || !count) return AFROS_ERROR_INVALID_PARAM;
    *out   = (segment_command_64_t *)img->segments;
    *count = img->nsegments;
    return AFROS_SUCCESS;
}

afros_status_t MachoGetSections(macho_image_t *img, const char *segname,
                                const char *sectname, section_64_t **out) {
    if (!img || !out) return AFROS_ERROR_INVALID_PARAM;
    *out = NULL;
    for (uint32_t s = 0; s < img->nsegments; s++) {
        const segment_command_64_t *seg = img->segments[s];
        if (segname && strncmp(seg->segname, segname, 16) != 0) continue;
        const section_64_t *sects =
            (const section_64_t *)(seg + 1);
        for (uint32_t i = 0; i < seg->nsects; i++) {
            if (sectname && strncmp(sects[i].sectname, sectname, 16) != 0)
                continue;
            *out = (section_64_t *)&sects[i];
            return AFROS_SUCCESS;
        }
    }
    return AFROS_ERROR;
}

void MachoRelease(macho_image_t *img) {
    if (!img) return;
    free((void *)img->cmds);
    free((void *)img->segments);
    free(img);
}

/* Return a load command by id (first match), or NULL.                */
const load_command_t *macho_find_cmd(macho_image_t *img, uint32_t cmd_id) {
    if (!img) return NULL;
    for (uint32_t i = 0; i < img->ncmds; i++) {
        if (macho_read32(&img->cmds[i]->cmd, img->swap) == cmd_id) {
            return img->cmds[i];
        }
    }
    return NULL;
}

uint32_t macho_image_filetype(const macho_image_t *img) {
    return img ? img->filetype : 0;
}

uint32_t macho_image_cputype(const macho_image_t *img) {
    return img ? img->cputype : 0;
}

const symtab_command_t *macho_image_symtab(const macho_image_t *img) {
    return img ? img->symtab : NULL;
}

const dysymtab_command_t *macho_image_dysymtab(const macho_image_t *img) {
    return img ? img->dysymtab : NULL;
}

const void *macho_image_base(const macho_image_t *img) {
    return img ? img->base : NULL;
}

size_t macho_image_size(const macho_image_t *img) {
    return img ? img->size : 0;
}
