#include "../include/afros_net.h"
#include <stdio.h>
#include <string.h>

/**
 * @file afros_net.c
 * @brief Implementation of the intelligent network manager for AfriOS.
 */

static bool g_net_initialized = false;
static bool g_energy_saving = false;

afros_status_t net_init(void) {
    if (g_net_initialized) {
        return AFROS_SUCCESS;
    }

    printf("AfriOS Network: Initializing intelligent networking subsystem...\n");
    // In a real implementation, this would detect available interfaces (WiFi, Mobile, etc.)
    
    g_net_initialized = true;
    return AFROS_SUCCESS;
}

afros_status_t net_send_packet(afros_net_interface_t type, const uint8_t *data, size_t size) {
    if (!g_net_initialized) {
        return AFROS_ERROR;
    }

    const char *interface_name = "UNKNOWN";
    switch (type) {
        case NET_TYPE_ETHERNET: interface_name = "ETHERNET"; break;
        case NET_TYPE_WIFI:     interface_name = "WIFI"; break;
        case NET_TYPE_MOBILE:   interface_name = "MOBILE"; break;
        case NET_TYPE_SATELLITE: interface_name = "SATELLITE"; break;
    }

    printf("AfriOS Network: Sending packet (%zu bytes) via %s\n", size, interface_name);

    // Apply optimization logic based on interface type and energy mode
    if (g_energy_saving && type == NET_TYPE_MOBILE) {
        printf("AfriOS Network: [Optimization] Batching packet to save power on mobile interface.\n");
    }

    if (type == NET_TYPE_SATELLITE) {
        printf("AfriOS Network: [Optimization] Using high-latency protocol optimization for satellite link.\n");
    }

    // Simulate hardware transmission
    return AFROS_SUCCESS;
}

afros_status_t net_optimize_bandwidth(bool energy_saving_mode) {
    if (!g_net_initialized) {
        return AFROS_ERROR;
    }

    g_energy_saving = energy_saving_mode;
    printf("AfriOS Network: Bandwidth optimization set to %s mode.\n", 
           energy_saving_mode ? "ENERGY-SAVING" : "PERFORMANCE");

    return AFROS_SUCCESS;
}
