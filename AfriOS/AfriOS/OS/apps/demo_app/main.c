/* Use the public AfriOS SDK umbrella header — provides access to all subsystems
 * (storage, network, power, UI) via a single include. Path-agnostic: the SDK
 * target propagates its include directory via target_include_directories(). */
#include "afros_sdk.h"
#include "afros_net.h"
#include "storage_mgr.h"
#include "afros_power.h"
#include <stdio.h>

/**
 * @file main.c
 * @brief Application de démonstration pour valider les sous-systèmes AfriOS.
 */

int main() {
    printf("=== AfriOS Demo Application ===\n\n");

    // 1. Initialisation des services
    printf("[APP] Initialisation des services...\n");
    storage_init();
    net_init();
    power_manager_init();

    // 2. Utilisation du Stockage (AfrosFS)
    printf("\n--- Test Stockage ---\n");
    const char *data = "Données confidentielles AfriOS";
    storage_get_ops()->enable_encryption(true, "AFROS_SECURE_KEY_2026");
    storage_get_ops()->write_file("/home/user/secret.txt", (uint8_t*)data, 30);

    // 3. Utilisation du Réseau Intelligent
    printf("\n--- Test Réseau ---\n");
    net_optimize_bandwidth(true); // Mode économie d'énergie
    net_send_packet(NET_TYPE_SATELLITE, (uint8_t*)"PING", 4);

    // 4. Utilisation de l'Énergie
    printf("\n--- Test Énergie ---\n");
    /* power_monitor_battery() is a legacy name — the public API is
     * power_is_on_ac_power() + power_get_current_state(). */
    printf("[APP] Sur AC ? %s\n", power_is_on_ac_power() ? "oui" : "non");
    printf("[APP] État actuel : %u\n", (unsigned)power_get_current_state());

    // 5. Interface Utilisateur via le SDK
    printf("\n--- Test UI SDK ---\n");
    afros_ui_create_window("AfriOS Control Center", 800, 600);
    afros_ui_draw_text("Système Opérationnel", 100, 50);

    printf("\n=== Demo terminée avec succès ===\n");
    return 0;
}
