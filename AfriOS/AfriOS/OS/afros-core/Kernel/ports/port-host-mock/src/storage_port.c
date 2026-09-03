/*
 * storage_port.c — Host-mock block storage operations.
 *
 * Backed by a regular file on /tmp (override with $AFROS_HOST_MOCK_STORAGE_PATH):
 *   - init          -> opens (creates if missing) a 1 MiB backing file
 *   - get_info      -> fills afros_storage_info_t with mock geometry
 *                      (block_size=512, block_count=2048, read_only=false)
 *   - read_blocks   -> fseek + fread
 *   - write_blocks  -> fseek + fwrite
 *   - flush         -> fflush
 *
 * This is the only port where read_blocks/write_blocks actually succeed —
 * on x86_64/arm64/riscv the storage port is a stub that returns
 * NOT_SUPPORTED, and on port-mcu write_blocks is hard-wired to NOT_SUPPORTED
 * (zone code is read-only). The host-mock lets the HAL test runner take
 * the SUCCESS branch on every storage op, which is the whole point of
 * having a host-mock.
 */
#include "storage_abstraction.h"
#include "port_host_mock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

static FILE     *s_storage_fp       = NULL;
static uint32_t  s_block_size       = AFROS_HOST_MOCK_STORAGE_BLOCK_SIZE;
static uint64_t  s_block_count      = AFROS_HOST_MOCK_STORAGE_BLOCK_COUNT;
static const char *s_storage_path   = AFROS_HOST_MOCK_STORAGE_PATH_DEFAULT;

/* Ensure the backing file exists and is at least s_block_count *
 * s_block_size bytes. Creates it (zero-filled) if missing, and grows it
 * with ftruncate if it's too short. Idempotent — safe to call from init
 * on every run. */
static afros_status_t ensure_backing_file(void) {
    const char *env_path = getenv("AFROS_HOST_MOCK_STORAGE_PATH");
    if (env_path && *env_path) {
        s_storage_path = env_path;
    }

    /* Try to open existing file read/write first. */
    FILE *fp = fopen(s_storage_path, "r+b");
    if (fp == NULL) {
        /* Doesn't exist (or no permission) — create it. */
        fp = fopen(s_storage_path, "w+b");
        if (fp == NULL) {
            return AFROS_ERROR_IO;
        }
    }

    /* Grow to full size if needed. */
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return AFROS_ERROR_IO; }
    long cur_size = ftell(fp);
    if (cur_size < 0) { fclose(fp); return AFROS_ERROR_IO; }
    long want_size = (long)(s_block_count * s_block_size);
    if (cur_size < want_size) {
        /* Write zeros until the file is want_size bytes. ftruncate is
         * simpler but only works on regular files — we use it where
         * possible and fall back to fwrite for portability. */
        if (fseek(fp, want_size - 1, SEEK_SET) != 0) {
            fclose(fp); return AFROS_ERROR_IO;
        }
        if (fputc(0, fp) == EOF) {
            fclose(fp); return AFROS_ERROR_IO;
        }
        fflush(fp);
    }

    s_storage_fp = fp;
    return AFROS_SUCCESS;
}

static afros_status_t storage_init_impl(void) {
    if (s_storage_fp != NULL) {
        /* Already initialised — idempotent. */
        return AFROS_SUCCESS;
    }
    return ensure_backing_file();
}

static afros_status_t storage_get_info_impl(uint32_t device_id, afros_storage_info_t *info) {
    (void)device_id;
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    info->block_size  = s_block_size;
    info->block_count = s_block_count;
    info->read_only   = AFROS_HOST_MOCK_STORAGE_READ_ONLY;
    return AFROS_SUCCESS;
}

static afros_status_t storage_read_blocks_impl(uint32_t device_id, uint64_t lba,
                                               uint32_t count, uint8_t *buffer) {
    (void)device_id;
    if (!buffer)     return AFROS_ERROR_INVALID_PARAM;
    if (count == 0)  return AFROS_SUCCESS;
    if (s_storage_fp == NULL) {
        afros_status_t s = storage_init_impl();
        if (s != AFROS_SUCCESS) return s;
    }

    /* Bounds-check: refuse reads past the end of the backing file. */
    if (lba + count > s_block_count) return AFROS_ERROR_INVALID_PARAM;

    long offset = (long)(lba * s_block_size);
    if (fseek(s_storage_fp, offset, SEEK_SET) != 0) return AFROS_ERROR_IO;

    size_t want = (size_t)(count * s_block_size);
    size_t got  = fread(buffer, 1, want, s_storage_fp);
    if (got != want) {
        /* Short read — pad with zeros so the caller gets deterministic
         * content even if the backing file is sparse (it is, after the
         * ftruncate-style growth in ensure_backing_file). */
        if (ferror(s_storage_fp)) return AFROS_ERROR_IO;
        memset(buffer + got, 0, want - got);
    }
    return AFROS_SUCCESS;
}

static afros_status_t storage_write_blocks_impl(uint32_t device_id, uint64_t lba,
                                                uint32_t count, const uint8_t *buffer) {
    (void)device_id;
    if (!buffer)     return AFROS_ERROR_INVALID_PARAM;
    if (count == 0)  return AFROS_SUCCESS;
    if (s_storage_fp == NULL) {
        afros_status_t s = storage_init_impl();
        if (s != AFROS_SUCCESS) return s;
    }

    if (lba + count > s_block_count) return AFROS_ERROR_INVALID_PARAM;

    long offset = (long)(lba * s_block_size);
    if (fseek(s_storage_fp, offset, SEEK_SET) != 0) return AFROS_ERROR_IO;

    size_t want = (size_t)(count * s_block_size);
    size_t put  = fwrite(buffer, 1, want, s_storage_fp);
    if (put != want) return AFROS_ERROR_IO;
    return AFROS_SUCCESS;
}

static afros_status_t storage_flush_impl(uint32_t device_id) {
    (void)device_id;
    if (s_storage_fp == NULL) return AFROS_SUCCESS; /* nothing to flush */
    if (fflush(s_storage_fp) != 0) return AFROS_ERROR_IO;
    return AFROS_SUCCESS;
}

storage_ops_t arch_storage_ops = {
    .init         = storage_init_impl,
    .get_info     = storage_get_info_impl,
    .read_blocks  = storage_read_blocks_impl,
    .write_blocks = storage_write_blocks_impl,
    .flush        = storage_flush_impl
};
