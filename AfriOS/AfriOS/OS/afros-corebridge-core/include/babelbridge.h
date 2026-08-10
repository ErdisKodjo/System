#ifndef AFROS_BABELBRIDGE_H
#define AFROS_BABELBRIDGE_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file babelbridge.h
 * @brief Universal bridge for cross-platform API translation in AfriOS.
 */

afros_status_t babel_init(void);
afros_status_t babel_translate_api(const char *source_framework, const char *target_framework, const char *api_name);

#endif
