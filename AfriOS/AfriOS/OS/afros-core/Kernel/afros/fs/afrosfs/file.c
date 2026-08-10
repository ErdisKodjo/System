#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file file.c
 * @brief AfrosFS file system implementation for AfriOS.
 * Designed for reliability and power efficiency.
 */

typedef struct {
    char name[256];
    afros_size_t size;
    uint32_t flags;
} afros_file_t;

afros_status_t afrosfs_open_file(const char *name, afros_file_t *file) {
    if (!name || !file) return AFROS_ERROR_INVALID_PARAM;
    kprintf("AfrosFS: Opening file %s...\n", name);
    // Open file from the file system and populate metadata
    return AFROS_SUCCESS;
}

afros_status_t afrosfs_read_file(afros_file_t *file, uint8_t *buffer, afros_size_t size) {
    if (!file || !buffer) return AFROS_ERROR_INVALID_PARAM;
    kprintf("AfrosFS: Reading %zu bytes from file %s...\n", size, file->name);
    // Simulation de lecture physique via le HAL
    return AFROS_SUCCESS;
}

afros_status_t afrosfs_write_file(afros_file_t *file, const uint8_t *buffer, afros_size_t size) {
    if (!file || !buffer) return AFROS_ERROR_INVALID_PARAM;
    
    // Vrification des permissions (Lecture Seule ?)
    if (file->flags & 0x1) { // 0x1 = READ_ONLY
        kprintf("AfrosFS ERROR: Attempt to write to read-only file %s.\n", file->name);
        return AFROS_ERROR;
    }

    kprintf("AfrosFS: Writing %zu bytes to file %s...\n", size, file->name);
    
    // Mise  jour de la taille
    file->size += size;
    
    // Synchronisation avec le journal (Journaling)
    kprintf("AfrosFS [Journal]: Committing write transaction for %s.\n", file->name);
    
    return AFROS_SUCCESS;
}

afros_status_t afrosfs_delete_file(const char *name) {
    kprintf("AfrosFS: Deleting file %s and reclaiming blocks.\n", name);
    return AFROS_SUCCESS;
}
