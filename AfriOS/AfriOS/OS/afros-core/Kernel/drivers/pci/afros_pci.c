#include "afros_hal.h"

/**
 * @file afros_pci.c
 * @brief PCI driver for AfriOS.
 * Provides access to PCI devices.
 *
 * Étape 4 : s'enregistre désormais réellement auprès d'arch_device_manager_ops
 * (device_manager.c) via device_ops_t, au lieu d'exposer des fonctions
 * pci_init()/pci_scan_bus() déconnectées du reste de la HAL.
 */

static afros_status_t pci_dev_init(uint32_t device_id) {
    (void)device_id;
    // Initializing PCI bus
    return AFROS_SUCCESS;
}

static afros_status_t pci_dev_ioctl(uint32_t device_id, uint32_t command, void *args) {
    (void)device_id; (void)command; (void)args;
    // Accès à l'espace de configuration PCI (lecture/écriture de BAR, etc.) :
    // pas encore implémenté, à traiter avec un port réel (accès MMIO/PIO).
    return AFROS_ERROR_NOT_SUPPORTED;
}

static device_ops_t pci_host_ops = {
    .device_id = 0,
    .name = "pci-host",
    .init = pci_dev_init,
    .read = NULL,
    .write = NULL,
    .ioctl = pci_dev_ioctl
};

void pci_init(void) {
    arch_device_manager_ops.register_device(&pci_host_ops);
}

void pci_scan_bus(void) {
    // Enumerate and register PCI devices found on the bus
}
