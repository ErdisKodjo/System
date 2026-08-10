#include "../include/afros_net.h"
#include "kprintf.h"

/**
 * @file afros_net.c
 * @brief Implementation of the intelligent network manager for AfriOS.
 *
 * Freestanding: utilise kprintf (HAL) au lieu de <stdio.h>/printf. Les
 * messages sont émis via arch_console_ops (PL011/SBI/USART/16550 selon le
 * port actif) une fois la HAL initialisée — avant, kprintf devient no-op
 * mais ne crash pas.
 */

static bool g_net_initialized = false;
static bool g_energy_saving = false;

afros_status_t net_init(void) {
    if (g_net_initialized) {
        return AFROS_SUCCESS;
    }

    kprintf("AfriOS Network: Initializing intelligent networking subsystem...\n");
    /* In a real implementation, this would detect available interfaces
     * (WiFi, Mobile, etc.) via the HAL device manager. */

    g_net_initialized = true;
    return AFROS_SUCCESS;
}

afros_status_t net_send_packet(afros_net_interface_t type, const uint8_t *data, size_t size) {
    if (!g_net_initialized) {
        return AFROS_ERROR;
    }
    (void)data;

    const char *interface_name = "UNKNOWN";
    switch (type) {
        case NET_TYPE_ETHERNET:  interface_name = "ETHERNET";  break;
        case NET_TYPE_WIFI:      interface_name = "WIFI";      break;
        case NET_TYPE_MOBILE:    interface_name = "MOBILE";    break;
        case NET_TYPE_SATELLITE: interface_name = "SATELLITE"; break;
    }

    kprintf("AfriOS Network: Sending packet (%zu bytes) via %s\n", size, interface_name);

    /* Apply optimization logic based on interface type and energy mode */
    if (g_energy_saving && type == NET_TYPE_MOBILE) {
        kprintf("AfriOS Network: [Optimization] Batching packet to save power on mobile interface.\n");
    }

    if (type == NET_TYPE_SATELLITE) {
        kprintf("AfriOS Network: [Optimization] Using high-latency protocol optimization for satellite link.\n");
    }

    /* Simulate hardware transmission */
    return AFROS_SUCCESS;
}

afros_status_t net_optimize_bandwidth(bool energy_saving_mode) {
    if (!g_net_initialized) {
        return AFROS_ERROR;
    }

    g_energy_saving = energy_saving_mode;
    kprintf("AfriOS Network: Bandwidth optimization set to %s mode.\n",
            energy_saving_mode ? "ENERGY-SAVING" : "PERFORMANCE");

    return AFROS_SUCCESS;
}
