#ifndef AFROS_PKG_MGR_H
#define AFROS_PKG_MGR_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file pkg_mgr.h
 * @brief Gestionnaire de paquets universel (APKG) pour AfriOS.
 */

typedef enum {
    PKG_TYPE_NATIVE,
    PKG_TYPE_WINBRIDGE,
    PKG_TYPE_ANDROID
} afros_pkg_type_t;

typedef struct {
    char name[64];
    char version[16];
    afros_pkg_type_t type;
} afros_package_t;

afros_status_t pkg_init(void);
afros_status_t pkg_install(const char *pkg_path, afros_pkg_type_t type);
afros_status_t pkg_launch(const char *pkg_name);

#endif
