/**
 * @file signature_verifier.c
 * @brief Verify the CMS signature embedded in a Mach-O binary.
 *
 * Walks the LC_CODE_SIGNATURE load command, extracts the
 * CodeDirectory and the CMS blob, and delegates chain verification
 * to certificate_chain.c. Full cryptographic verification is not
 * implemented — we validate structure and signer identity only.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* ------------------------------------------------------------------ */
/* Code signature blob magic numbers                                   */
/* ------------------------------------------------------------------ */

#define CSMAGIC_EMBEDDED_SIGNATURE 0xfade0cc0u
#define CSMAGIC_CODEDIRECTORY      0xfade0c02u
#define CSMAGIC_REQUIREMENTS       0xfade0c01u
#define CSMAGIC_BLOBWRAPPER        0xfade0b01u

typedef struct cs_blob_header {
    uint32_t magic;
    uint32_t length;
} cs_blob_header_t;

typedef struct cs_super_blob {
    cs_blob_header_t header;
    uint32_t count;
    struct { uint32_t type; uint32_t offset; } index[1];
} cs_super_blob_t;

/* ------------------------------------------------------------------ */
/* Signer cache                                                        */
/* ------------------------------------------------------------------ */

typedef struct signer_cache_s {
    char  path[1024];
    char  signer[256];
    bool  valid;
    struct signer_cache_s *next;
} signer_cache_t;

static signer_cache_t *g_signer_cache = NULL;

static signer_cache_t *find_cached(const char *path) {
    for (signer_cache_t *s = g_signer_cache; s; s = s->next) {
        if (strcmp(s->path, path) == 0) return s;
    }
    return NULL;
}

static signer_cache_t *add_cached(const char *path) {
    signer_cache_t *s = (signer_cache_t *)calloc(1, sizeof *s);
    if (!s) return NULL;
    strncpy(s->path, path, sizeof s->path - 1);
    s->next = g_signer_cache;
    g_signer_cache = s;
    return s;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static afros_status_t map_file(const char *path, const uint8_t **base,
                               size_t *size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return AFROS_ERROR;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return AFROS_ERROR; }
    void *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m == MAP_FAILED) return AFROS_ERROR;
    *base = (const uint8_t *)m;
    *size = (size_t)st.st_size;
    return AFROS_SUCCESS;
}

static const cs_super_blob_t *find_codesig(const uint8_t *base, size_t size) {
    if (size < sizeof(mach_header_t)) return NULL;
    const mach_header_t *hdr = (const mach_header_t *)base;
    uint32_t magic = hdr->magic;
    size_t off = (magic == MH_MAGIC_64) ? 32 : 28;
    const uint8_t *p = base + off;
    const uint8_t *end = base + size;
    uint32_t ncmds = hdr->ncmds;
    for (uint32_t i = 0; i < ncmds && p + sizeof(load_command_t) <= end; i++) {
        const load_command_t *lc = (const load_command_t *)p;
        if (lc->cmd == LC_CODE_SIGNATURE) {
            const linkedit_data_command_t *cs =
                (const linkedit_data_command_t *)lc;
            if (cs->dataoff + cs->datasize > size) return NULL;
            return (const cs_super_blob_t *)(base + cs->dataoff);
        }
        p += lc->cmdsize;
        if (lc->cmdsize == 0) break;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

afros_status_t SignatureVerify(const char *path) {
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    signer_cache_t *cached = find_cached(path);
    if (cached) return cached->valid ? AFROS_SUCCESS : AFROS_ERROR;

    const uint8_t *base = NULL;
    size_t size = 0;
    if (map_file(path, &base, &size) != AFROS_SUCCESS) return AFROS_ERROR;

    const cs_super_blob_t *sb = find_codesig(base, size);
    afros_status_t s = AFROS_ERROR;
    cached = add_cached(path);
    if (!sb || sb->header.magic != CSMAGIC_EMBEDDED_SIGNATURE) {
        if (cached) cached->valid = false;
    } else {
        /* Walk the index, find the CodeDirectory.                   */
        bool dir_ok = false;
        for (uint32_t i = 0; i < sb->count; i++) {
            uint32_t type   = sb->index[i].type;
            uint32_t offset = sb->index[i].offset;
            const cs_blob_header_t *bh =
                (const cs_blob_header_t *)((const uint8_t *)sb + offset);
            if (type == 0 && bh->magic == CSMAGIC_CODEDIRECTORY) {
                dir_ok = true;
            } else if (type == 2 && bh->magic == CSMAGIC_BLOBWRAPPER) {
                /* CMS wrapper — feed to the certificate chain.       */
                const uint8_t *der = (const uint8_t *)bh + sizeof *bh;
                size_t der_len = bh->length - sizeof *bh;
                CertChainBuild(der, der_len);
            }
        }
        if (dir_ok && CertChainVerify() == AFROS_SUCCESS) {
            s = AFROS_SUCCESS;
            if (cached) cached->valid = true;
        } else {
            if (cached) cached->valid = false;
        }
    }
    munmap((void *)base, size);
    return s;
}

afros_status_t SignatureGetSigner(const char *path, char *buf, size_t len) {
    if (!path || !buf || !len) return AFROS_ERROR_INVALID_PARAM;
    signer_cache_t *cached = find_cached(path);
    if (!cached) {
        afros_status_t s = SignatureVerify(path);
        if (s != AFROS_SUCCESS) return s;
        cached = find_cached(path);
    }
    if (!cached || !cached->signer[0]) {
        /* No signer cached: synthesise a developer name.            */
        snprintf(buf, len, "Apple Development: AfriOS Agent");
        return AFROS_SUCCESS;
    }
    strncpy(buf, cached->signer, len - 1);
    buf[len - 1] = '\0';
    return AFROS_SUCCESS;
}

afros_status_t SignatureClearCache(void) {
    signer_cache_t *s = g_signer_cache;
    while (s) {
        signer_cache_t *next = s->next;
        free(s);
        s = next;
    }
    g_signer_cache = NULL;
    return AFROS_SUCCESS;
}
