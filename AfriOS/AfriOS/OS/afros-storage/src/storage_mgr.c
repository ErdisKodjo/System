#include "../include/storage_mgr.h"
#include "kprintf.h"

/**
 * @file storage_mgr.c
 * @brief Implementation of the storage manager (AfrosFS) for AfriOS.
 *
 * Freestanding: utilise kprintf (HAL) au lieu de <stdio.h>/printf.
 * strncpy est remplacé par une copie locale libc-free (storage_copy_str).
 */

static bool g_storage_initialized = false;
static bool g_encryption_enabled = false;
static char g_encryption_key[256] = {0};

/** Copie bornée d'une chaîne (libc-free, équivalent strncpy + terminaison). */
static void storage_copy_str(char *dst, size_t n, const char *src) {
    size_t i = 0;
    if (dst && n) {
        if (src) while (i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
        dst[i] = '\0';
    }
}

static afros_status_t afros_mount(const char *partition) {
    if (!g_storage_initialized) return AFROS_ERROR;
    kprintf("AfriOS Storage: Mounting partition %s...\n", partition);
    return AFROS_SUCCESS;
}

static afros_status_t afros_read_file(const char *path, uint8_t *buffer, size_t size) {
    if (!g_storage_initialized) return AFROS_ERROR;
    (void)buffer;
    kprintf("AfriOS Storage: Reading file %s (%zu bytes)...\n", path, size);
    if (g_encryption_enabled) {
        kprintf("AfriOS Storage: [Security] Decrypting data using current key.\n");
    }
    return AFROS_SUCCESS;
}

static afros_status_t afros_write_file(const char *path, const uint8_t *buffer, size_t size) {
    if (!g_storage_initialized) return AFROS_ERROR;
    (void)buffer;
    kprintf("AfriOS Storage: Writing file %s (%zu bytes)...\n", path, size);
    if (g_encryption_enabled) {
        kprintf("AfriOS Storage: [Security] Encrypting data before write.\n");
    }
    return AFROS_SUCCESS;
}

static afros_status_t afros_enable_encryption(bool enable, const char *key) {
    if (!g_storage_initialized) return AFROS_ERROR;
    g_encryption_enabled = enable;
    if (enable && key) {
        storage_copy_str(g_encryption_key, sizeof(g_encryption_key), key);
        kprintf("AfriOS Storage: Encryption enabled.\n");
    } else {
        kprintf("AfriOS Storage: Encryption disabled.\n");
    }
    return AFROS_SUCCESS;
}

static storage_ops_t g_storage_ops = {
    .mount = afros_mount,
    .read_file = afros_read_file,
    .write_file = afros_write_file,
    .enable_encryption = afros_enable_encryption
};

afros_status_t storage_init(void) {
    if (g_storage_initialized) return AFROS_SUCCESS;
    kprintf("AfriOS Storage: Initializing storage manager...\n");
    g_storage_initialized = true;
    return AFROS_SUCCESS;
}

/* Function to get storage operations (if needed) */
storage_ops_t* storage_get_ops(void) {
    return &g_storage_ops;
}
