#ifndef AFROS_NET_H
#define AFROS_NET_H

/*
 * En-tête HAL fourni transitivement par la cible CMake `afros-hal`
 * (target_link_libraries afros-network PUBLIC afros-hal). En cas de build
 * partiel sans la cible, le CMakeLists de afros-network repli sur
 * ${CMAKE_SOURCE_DIR}/afros-core/Kernel/hal/include comme include direct.
 *
 * On inclut ici uniquement afros_types.h (types de base) plutôt que afros_hal.h
 * (qui amènerait cpu_abstraction, memory_abstraction, … inutiles au réseau).
 */
#include "afros_types.h"

/**
 * @file afros_net.h
 * @brief Gestionnaire réseau intelligent pour AfriOS.
 */

typedef enum {
    NET_TYPE_ETHERNET,
    NET_TYPE_WIFI,
    NET_TYPE_MOBILE,
    NET_TYPE_SATELLITE
} afros_net_interface_t;

afros_status_t net_init(void);
afros_status_t net_send_packet(afros_net_interface_t type, const uint8_t *data, size_t size);
afros_status_t net_optimize_bandwidth(bool energy_saving_mode);

#endif
