#ifndef AFROS_HARMONY_H
#define AFROS_HARMONY_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file afros_harmony.h
 * @brief HarmonyOS compatibility layer for AfriOS.
 */

typedef struct {
    afros_status_t (*start_ability)(const char *ability_name);
    afros_status_t (*manage_lifecycle)(uint32_t state);
} harmony_compat_ops_t;

afros_status_t harmony_init(void);
afros_status_t harmony_launch_app(const char *hap_path);

#endif
