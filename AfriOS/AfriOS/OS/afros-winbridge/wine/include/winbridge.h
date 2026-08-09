#ifndef AFROS_WINBRIDGE_H
#define AFROS_WINBRIDGE_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file winbridge.h
 * @brief Interface de compatibilité Windows pour AfriOS.
 */

typedef struct {
    afros_status_t (*load_pe)(const char *path);
    afros_status_t (*translate_syscall)(uint32_t nt_syscall_id);
    afros_status_t (*emulate_registry_read)(const char *key, char *value_out);
} winbridge_ops_t;

afros_status_t winbridge_init(void);
afros_status_t winbridge_launch_exe(const char *path);

#endif
