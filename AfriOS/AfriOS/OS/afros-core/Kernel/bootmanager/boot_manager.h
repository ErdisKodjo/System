/**
 * @file boot_manager.h
 * @brief BootManager UEFI pour AfriOS - Gestion du démarrage multi-OS
 */

#ifndef AFROS_BOOT_MANAGER_H
#define AFROS_BOOT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/afros_types.h"

// Types de boot entries
typedef enum {
    BOOT_ENTRY_AFRIOS,
    BOOT_ENTRY_WINDOWS,
    BOOT_ENTRY_LINUX,
    BOOT_ENTRY_MACOS,
    BOOT_ENTRY_ANDROID,
    BOOT_ENTRY_PXE,
    BOOT_ENTRY_UEFI_SHELL,
    BOOT_ENTRY_DIAGNOSTICS
} boot_entry_type_t;

// Structure d'une entrée de boot
typedef struct {
    uint32_t id;
    boot_entry_type_t type;
    char label[64];
    char device_path[256];
    char arguments[128];
    bool is_default;
    bool is_active;
    uint32_t timeout;
} boot_entry_t;

// Configuration du BootManager
typedef struct {
    boot_entry_t* entries;
    uint32_t entry_count;
    uint32_t default_entry;
    uint32_t timeout_seconds;
    bool auto_boot;
    bool show_menu;
} boot_manager_config_t;

// Fonctions principales
afros_status_t boot_manager_init(void);
afros_status_t boot_manager_add_entry(boot_entry_t* entry);
afros_status_t boot_manager_remove_entry(uint32_t id);
afros_status_t boot_manager_set_default(uint32_t id);
afros_status_t boot_manager_boot_entry(uint32_t id);
afros_status_t boot_manager_display_menu(void);
afros_status_t boot_manager_auto_boot(void);
afros_status_t boot_manager_save_config(void);
afros_status_t boot_manager_load_config(void);

// Détection automatique des OS installés
afros_status_t boot_manager_detect_os(void);
afros_status_t boot_manager_scan_efi_partitions(void);
afros_status_t boot_manager_scan_mbr_partitions(void);

// Support spécifique par OS
afros_status_t boot_manager_prepare_windows_boot(boot_entry_t* entry);
afros_status_t boot_manager_prepare_linux_boot(boot_entry_t* entry);
afros_status_t boot_manager_prepare_macos_boot(boot_entry_t* entry);
afros_status_t boot_manager_prepare_android_boot(boot_entry_t* entry);
afros_status_t boot_manager_prepare_pxe_boot(boot_entry_t* entry);

#endif // AFROS_BOOT_MANAGER_H
