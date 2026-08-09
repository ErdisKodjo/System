#ifndef AFROS_STORAGE_MGR_H
#define AFROS_STORAGE_MGR_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file storage_mgr.h
 * @brief Gestionnaire de stockage (AfrosFS) avec chiffrement.
 */

typedef struct {
    afros_status_t (*mount)(const char *partition);
    afros_status_t (*read_file)(const char *path, uint8_t *buffer, size_t size);
    afros_status_t (*write_file)(const char *path, const uint8_t *buffer, size_t size);
    afros_status_t (*enable_encryption)(bool enable, const char *key);
} storage_ops_t;

afros_status_t storage_init(void);
storage_ops_t* storage_get_ops(void);

#endif
