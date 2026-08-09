/**
 * @file boot_manager.c
 * @brief Implémentation du BootManager UEFI pour AfriOS
 */

#include "boot_manager.h"
#include "../include/afros_types.h"
#include "../HAL/include/afros_hal.h"

// Configuration globale
static boot_manager_config_t g_boot_config = {
    .entries = NULL,
    .entry_count = 0,
    .default_entry = 0,
    .timeout_seconds = 5,
    .auto_boot = true,
    .show_menu = true
};

// Buffer pour les entrées de boot (max 16 OS détectés)
#define MAX_BOOT_ENTRIES 16
static boot_entry_t g_boot_entries[MAX_BOOT_ENTRIES];

/**
 * @brief Initialise le BootManager
 */
afros_status_t boot_manager_init(void) {
    afros_log_info("BootManager: Initialisation...\n");
    
    // Initialiser le tableau des entrées
    for (int i = 0; i < MAX_BOOT_ENTRIES; i++) {
        g_boot_entries[i].id = 0;
        g_boot_entries[i].type = BOOT_ENTRY_AFRIOS;
        g_boot_entries[i].is_active = false;
    }
    
    g_boot_config.entries = g_boot_entries;
    g_boot_config.entry_count = 0;
    
    // Charger la configuration depuis NVRAM
    afros_status_t status = boot_manager_load_config();
    if (status != AFROS_SUCCESS) {
        afros_log_warning("BootManager: Configuration non trouvée, utilisation des défauts\n");
    }
    
    // Détecter les OS installés
    boot_manager_detect_os();
    
    afros_log_info("BootManager: %d entrées de boot détectées\n", g_boot_config.entry_count);
    
    return AFROS_SUCCESS;
}

/**
 * @brief Ajoute une entrée de boot
 */
afros_status_t boot_manager_add_entry(boot_entry_t* entry) {
    if (g_boot_config.entry_count >= MAX_BOOT_ENTRIES) {
        afros_log_error("BootManager: Nombre maximum d'entrées atteint\n");
        return AFROS_ERROR_NO_MEMORY;
    }
    
    if (entry == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    
    // Assigner un ID unique
    entry->id = g_boot_config.entry_count + 1;
    
    // Copier l'entrée dans le tableau
    g_boot_entries[g_boot_config.entry_count] = *entry;
    g_boot_config.entry_count++;
    
    afros_log_info("BootManager: Entrée ajoutée - %s (ID: %d)\n", entry->label, entry->id);
    
    return AFROS_SUCCESS;
}

/**
 * @brief Supprime une entrée de boot
 */
afros_status_t boot_manager_remove_entry(uint32_t id) {
    if (id == 0 || id > g_boot_config.entry_count) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    
    // Décaler les entrées suivantes
    for (uint32_t i = id - 1; i < g_boot_config.entry_count - 1; i++) {
        g_boot_entries[i] = g_boot_entries[i + 1];
        g_boot_entries[i].id = i + 1;
    }
    
    g_boot_config.entry_count--;
    
    // Réinitialiser la dernière entrée
    g_boot_entries[g_boot_config.entry_count].id = 0;
    g_boot_entries[g_boot_config.entry_count].is_active = false;
    
    afros_log_info("BootManager: Entrée %d supprimée\n", id);
    
    return AFROS_SUCCESS;
}

/**
 * @brief Définit l'entrée de boot par défaut
 */
afros_status_t boot_manager_set_default(uint32_t id) {
    if (id == 0 || id > g_boot_config.entry_count) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    
    // Marquer toutes les entrées comme non-défaut
    for (uint32_t i = 0; i < g_boot_config.entry_count; i++) {
        g_boot_entries[i].is_default = false;
    }
    
    // Définir la nouvelle entrée par défaut
    g_boot_entries[id - 1].is_default = true;
    g_boot_config.default_entry = id - 1;
    
    afros_log_info("BootManager: Entrée par défaut définie à %d\n", id);
    
    return AFROS_SUCCESS;
}

/**
 * @brief Démarre une entrée de boot spécifique
 */
afros_status_t boot_manager_boot_entry(uint32_t id) {
    if (id == 0 || id > g_boot_config.entry_count) {
        afros_log_error("BootManager: ID d'entrée invalide\n");
        return AFROS_ERROR_INVALID_PARAM;
    }
    
    boot_entry_t* entry = &g_boot_entries[id - 1];
    
    if (!entry->is_active) {
        afros_log_error("BootManager: Entrée inactive\n");
        return AFROS_ERROR_INVALID_STATE;
    }
    
    afros_log_info("BootManager: Démarrage de %s...\n", entry->label);
    
    // Préparer le boot selon le type d'OS
    afros_status_t status = AFROS_SUCCESS;
    switch (entry->type) {
        case BOOT_ENTRY_WINDOWS:
            status = boot_manager_prepare_windows_boot(entry);
            break;
        case BOOT_ENTRY_LINUX:
            status = boot_manager_prepare_linux_boot(entry);
            break;
        case BOOT_ENTRY_MACOS:
            status = boot_manager_prepare_macos_boot(entry);
            break;
        case BOOT_ENTRY_ANDROID:
            status = boot_manager_prepare_android_boot(entry);
            break;
        case BOOT_ENTRY_PXE:
            status = boot_manager_prepare_pxe_boot(entry);
            break;
        case BOOT_ENTRY_AFRIOS:
        default:
            // Boot d'AfriOS lui-même (reboot)
            afros_log_info("BootManager: Reboot vers AfriOS\n");
            break;
    }
    
    if (status != AFROS_SUCCESS) {
        afros_log_error("BootManager: Échec de la préparation du boot\n");
        return status;
    }
    
    // Transférer le contrôle au système cible
    // Cette fonction ne retourne normalement pas
    afros_hal_reboot_to_image(entry->device_path);
    
    return AFROS_SUCCESS;
}

/**
 * @brief Affiche le menu de boot
 */
afros_status_t boot_manager_display_menu(void) {
    afros_log_info("\n=== AfriOS Boot Manager ===\n");
    
    for (uint32_t i = 0; i < g_boot_config.entry_count; i++) {
        boot_entry_t* entry = &g_boot_entries[i];
        const char* default_marker = entry->is_default ? " [DEFAULT]" : "";
        afros_log_info("%d. %s%s\n", i + 1, entry->label, default_marker);
    }
    
    afros_log_info("\nSélectionnez une option (défaut dans %ds): ", g_boot_config.timeout_seconds);
    
    // Attendre la sélection de l'utilisateur
    // TODO: Implémenter l'interface utilisateur graphique/textuelle
    
    return AFROS_SUCCESS;
}

/**
 * @brief Exécute le boot automatique
 */
afros_status_t boot_manager_auto_boot(void) {
    if (!g_boot_config.auto_boot) {
        return boot_manager_display_menu();
    }
    
    afros_log_info("BootManager: Boot automatique dans %d secondes...\n", g_boot_config.timeout_seconds);
    
    // Compte à rebours
    for (int i = g_boot_config.timeout_seconds; i > 0; i--) {
        afros_log_info("%d... ", i);
        // TODO: Vérifier si l'utilisateur a fait une sélection
        // hal_sleep_ms(1000);
    }
    
    afros_log_info("\n");
    
    // Démarrer l'entrée par défaut
    return boot_manager_boot_entry(g_boot_config.default_entry + 1);
}

/**
 * @brief Sauvegarde la configuration en NVRAM
 */
afros_status_t boot_manager_save_config(void) {
    afros_log_info("BootManager: Sauvegarde de la configuration...\n");
    
    // TODO: Écrire la configuration dans les variables UEFI NVRAM
    // efi_set_variable(L"AfriOSBootConfig", &g_boot_config, sizeof(g_boot_config));
    
    return AFROS_SUCCESS;
}

/**
 * @brief Charge la configuration depuis la NVRAM
 */
afros_status_t boot_manager_load_config(void) {
    afros_log_info("BootManager: Chargement de la configuration...\n");
    
    // TODO: Lire la configuration depuis les variables UEFI NVRAM
    // efi_get_variable(L"AfriOSBootConfig", &g_boot_config, sizeof(g_boot_config));
    
    // Pour l'instant, utiliser les valeurs par défaut
    return AFROS_SUCCESS;
}

/**
 * @brief Détecte automatiquement les OS installés
 */
afros_status_t boot_manager_detect_os(void) {
    afros_log_info("BootManager: Détection des OS installés...\n");
    
    // Scanner les partitions EFI
    boot_manager_scan_efi_partitions();
    
    // Scanner les partitions MBR (legacy)
    boot_manager_scan_mbr_partitions();
    
    // Ajouter AfriOS lui-même
    boot_entry_t afros_entry = {
        .type = BOOT_ENTRY_AFRIOS,
        .is_active = true,
        .is_default = true
    };
    snprintf(afros_entry.label, sizeof(afros_entry.label), "AfriOS");
    snprintf(afros_entry.device_path, sizeof(afros_entry.device_path), "\\EFI\\AFRIOS\\AFRIOS.EFI");
    boot_manager_add_entry(&afros_entry);
    
    return AFROS_SUCCESS;
}

/**
 * @brief Scanne les partitions EFI pour trouver des OS
 */
afros_status_t boot_manager_scan_efi_partitions(void) {
    afros_log_info("BootManager: Scan des partitions EFI...\n");
    
    // TODO: Implémenter le scan des partitions EFI
    // - Lister toutes les partitions avec le flag EFI_SYSTEM_PARTITION
    - Chercher les fichiers .EFI dans \\EFI\\BOOT\\, \\EFI\\MICROSOFT\\, etc.
    // - Créer des entrées de boot pour chaque OS trouvé
    
    // Exemple pour Windows
    boot_entry_t windows_entry = {
        .type = BOOT_ENTRY_WINDOWS,
        .is_active = true,
        .is_default = false
    };
    snprintf(windows_entry.label, sizeof(windows_entry.label), "Windows Boot Manager");
    snprintf(windows_entry.device_path, sizeof(windows_entry.device_path), "\\EFI\\MICROSOFT\\BOOT\\BOOTMGFW.EFI");
    // boot_manager_add_entry(&windows_entry); // Décommenter après détection réelle
    
    // Exemple pour Linux
    boot_entry_t linux_entry = {
        .type = BOOT_ENTRY_LINUX,
        .is_active = true,
        .is_default = false
    };
    snprintf(linux_entry.label, sizeof(linux_entry.label), "Linux (GRUB)");
    snprintf(linux_entry.device_path, sizeof(linux_entry.device_path), "\\EFI\\BOOT\\BOOTX64.EFI");
    // boot_manager_add_entry(&linux_entry); // Décommenter après détection réelle
    
    return AFROS_SUCCESS;
}

/**
 * @brief Scanne les partitions MBR pour les OS legacy
 */
afros_status_t boot_manager_scan_mbr_partitions(void) {
    afros_log_info("BootManager: Scan des partitions MBR...\n");
    
    // TODO: Implémenter le scan MBR
    // - Lire le secteur de boot (MBR)
    // - Identifier les partitions actives
    // - Détecter les bootloaders legacy (NTLDR, GRUB stage1, etc.)
    
    return AFROS_SUCCESS;
}

/**
 * @brief Prépare le boot pour Windows
 */
afros_status_t boot_manager_prepare_windows_boot(boot_entry_t* entry) {
    afros_log_info("BootManager: Préparation du boot Windows...\n");
    
    // Charger les variables UEFI spécifiques à Windows
    // - SecureBoot keys
    // - BootOrder
    // - LoaderPath
    
    // TODO: Implémenter la préparation spécifique Windows
    // - Vérifier SecureBoot
    // - Charger les drivers UEFI nécessaires
    // - Configurer BCD (Boot Configuration Data)
    
    return AFROS_SUCCESS;
}

/**
 * @brief Prépare le boot pour Linux
 */
afros_status_t boot_manager_prepare_linux_boot(boot_entry_t* entry) {
    afros_log_info("BootManager: Préparation du boot Linux...\n");
    
    // Charger le kernel Linux et initrd via LoadImage/StartImage
    // ou passer le contrôle à GRUB/shim
    
    // TODO: Implémenter la préparation spécifique Linux
    // - Parser les arguments du kernel
    // - Charger initrd si nécessaire
    // - Configurer Device Tree (pour ARM)
    
    return AFROS_SUCCESS;
}

/**
 * @brief Prépare le boot pour macOS
 */
afros_status_t boot_manager_prepare_macos_boot(boot_entry_t* entry) {
    afros_log_info("BootManager: Préparation du boot macOS...\n");
    
    // macOS nécessite des vérifications spécifiques:
    // - SecureBoot avec clés Apple
    // - T2 Security Chip (si présent)
    // - APFS snapshot validation
    
    // TODO: Implémenter la préparation spécifique macOS
    // - Vérifier les signatures Apple
    // - Charger OpenCore/Clover si nécessaire
    // - Configurer les propriétés ACPI
    
    return AFROS_SUCCESS;
}

/**
 * @brief Prépare le boot pour Android
 */
afros_status_t boot_manager_prepare_android_boot(boot_entry_t* entry) {
    afros_log_info("BootManager: Préparation du boot Android...\n");
    
    // Android sur x86_64 ou via compatibilité ARM
    // - Charger Android-x86 kernel
    // - Ou démarrer via émulation/virtualisation
    
    // TODO: Implémenter la préparation spécifique Android
    // - Charger le kernel Android
    // - Monter ramdisk Android
    // - Configurer les paramètres CMDLINE
    
    return AFROS_SUCCESS;
}

/**
 * @brief Prépare le boot PXE (réseau)
 */
afros_status_t boot_manager_prepare_pxe_boot(boot_entry_t* entry) {
    afros_log_info("BootManager: Préparation du boot PXE...\n");
    
    // Initialiser le réseau et DHCP
    // - Obtenir l'adresse IP
    // - Contacter le serveur TFTP
    // - Télécharger le fichier de boot (pxelinux.0, BOOTMGFW.EFI, etc.)
    
    // TODO: Implémenter le boot PXE
    // - Initialiser EFI_PXE_BASE_CODE_PROTOCOL
    // - Effectuer DHCP Discover/Offer/Request/Acknowledge
    // - Télécharger l'image via TFTP
    // - Charger et démarrer l'image
    
    return AFROS_SUCCESS;
}
