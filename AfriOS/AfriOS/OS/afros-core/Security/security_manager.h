/**
 * @file security_manager.h
 * @brief Gestionnaire de sécurité unifié pour AfriOS
 * 
 * Fournit une couche de sécurité complète incluant :
 * - Secure Boot et mesure d'intégrité
 * - Chiffrement disque (FDE) et fichiers (FBE)
 * - Contrôle d'accès obligatoire (MAC) et discret (DAC)
 * - Isolation des processus et sandboxing
 * - Audit et logging de sécurité
 */

#ifndef AFROS_SECURITY_MANAGER_H
#define AFROS_SECURITY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "afros_types.h"

// ============================================================================
// CONFIGURATION SECURE BOOT
// ============================================================================

typedef enum {
    SECURE_BOOT_DISABLED = 0,
    SECURE_BOOT_AUDIT,      // Log les violations sans bloquer
    SECURE_BOOT_ENFORCED,   // Bloque au premier échec
    SECURE_BOOT_DEPLOYED    // Mode production (clés verrouillées)
} secure_boot_mode_t;

typedef struct {
    uint8_t* pk_cert;       // Platform Key
    size_t pk_size;
    uint8_t* kek_cert;      // Key Exchange Keys
    size_t kek_size;
    uint8_t** db_certs;     // Signature Database
    size_t* db_sizes;
    size_t db_count;
    uint8_t** dbx_certs;    // Forbidden Signatures
    size_t* dbx_sizes;
    size_t dbx_count;
} secure_boot_keys_t;

typedef struct {
    uint8_t hash[32];       // SHA-256 du composant
    char component_name[64];
    uint64_t timestamp;
    bool verified;
} boot_measurement_t;

#define MAX_MEASUREMENTS 64

typedef struct {
    secure_boot_mode_t mode;
    secure_boot_keys_t keys;
    boot_measurement_t measurements[MAX_MEASUREMENTS];
    size_t measurement_count;
    bool tpm_present;
    uint32_t tpm_version;
} secure_boot_config_t;

// ============================================================================
// CHIFFREMENT (FDE & FBE)
// ============================================================================

typedef enum {
    CIPHER_AES_128_XTS = 0,
    CIPHER_AES_256_XTS,
    CIPHER_AES_128_GCM,
    CIPHER_AES_256_GCM,
    CIPHER_CHACHA20_POLY1305
} cipher_algorithm_t;

typedef enum {
    KDF_PBKDF2 = 0,
    KDF_SCRYPT,
    KDF_ARGON2I,
    KDF_ARGON2ID
} kdf_algorithm_t;

typedef struct {
    cipher_algorithm_t cipher;
    kdf_algorithm_t kdf;
    uint32_t key_size;          // bits
    uint32_t iterations;        // pour KDF
    uint8_t salt[32];
    uint8_t master_key[64];     // Chiffré avec TPM ou mot de passe
    uint64_t disk_size;
    uint64_t encrypted_sectors;
    bool fde_enabled;
    bool tpm_sealed;
} fde_config_t;

typedef struct {
    char file_path[256];
    cipher_algorithm_t cipher;
    uint8_t file_key[32];       // Unique par fichier
    uint64_t original_size;
    uint64_t encrypted_size;
    bool enabled;
} fbe_config_t;

// ============================================================================
// CONTRÔLE D'ACCÈS (MAC & DAC)
// ============================================================================

typedef enum {
    ACCESS_READ = (1 << 0),
    ACCESS_WRITE = (1 << 1),
    ACCESS_EXECUTE = (1 << 2),
    ACCESS_DELETE = (1 << 3),
    ACCESS_CHMOD = (1 << 4),
    ACCESS_CHOWN = (1 << 5),
    ACCESS_ADMIN = (1 << 6)
} access_flags_t;

typedef enum {
    SECURITY_LEVEL_LOW = 0,
    SECURITY_LEVEL_NORMAL,
    SECURITY_LEVEL_HIGH,
    SECURITY_LEVEL_SYSTEM,
    SECURITY_LEVEL_KERNEL
} security_level_t;

typedef struct {
    uint32_t uid;
    uint32_t gid;
    uint32_t* supplementary_gids;
    size_t gids_count;
    security_level_t level;
    uint64_t capabilities;      // Bitmask des capacités
} process_security_ctx_t;

typedef struct {
    char resource_path[256];
    uint32_t owner_uid;
    uint32_t owner_gid;
    uint16_t mode;              // Permissions UNIX
    uint8_t security_label[64]; // SELinux-style label
    access_flags_t mac_permissions;
    bool audit_enabled;
} resource_security_ctx_t;

typedef enum {
    POLICY_PERMISSIVE = 0,
    POLICY_ENFORCING
} policy_mode_t;

typedef struct {
    policy_mode_t mode;
    char policy_name[64];
    uint32_t policy_version;
    void* policy_data;          // Structure opaque de politique
    size_t policy_size;
} mac_policy_t;

// ============================================================================
// SANDBOXING & ISOLATION
// ============================================================================

typedef enum {
    SANDBOX_NONE = 0,
    SANDBOX_RESTRICTED,         // Limitation syscalls
    SANDBOX_ISOLATED,           // Namespace + cgroups
    SANDBOX_SECCOMP,            // Filtrage strict syscalls
    SANDBOX_VM                  // Micro-VM légère
} sandbox_level_t;

typedef struct {
    sandbox_level_t level;
    uint64_t allowed_syscalls;  // Bitmask
    char* allowed_paths[32];    // whitelist
    size_t paths_count;
    uint64_t max_memory;        // 0 = illimité
    uint64_t max_cpu_percent;   // 0-100
    uint64_t max_network_bps;   // 0 = illimité
    bool network_disabled;
    bool ipc_disabled;
    bool filesystem_write_disabled;
} sandbox_config_t;

// ============================================================================
// AUDIT & LOGGING
// ============================================================================

typedef enum {
    AUDIT_EVENT_BOOT = 0,
    AUDIT_EVENT_AUTH,
    AUDIT_EVENT_ACCESS,
    AUDIT_EVENT_POLICY_CHANGE,
    AUDIT_EVENT_KEY_OPERATION,
    AUDIT_EVENT_PROCESS_START,
    AUDIT_EVENT_NETWORK,
    AUDIT_EVENT_FILESYSTEM,
    AUDIT_EVENT_ERROR,
    AUDIT_EVENT_INTRUSION
} audit_event_type_t;

typedef enum {
    AUDIT_SEVERITY_INFO = 0,
    AUDIT_SEVERITY_WARNING,
    AUDIT_SEVERITY_ERROR,
    AUDIT_SEVERITY_CRITICAL
} audit_severity_t;

typedef struct {
    uint64_t event_id;
    audit_event_type_t type;
    audit_severity_t severity;
    uint64_t timestamp;
    uint32_t process_id;
    uint32_t user_id;
    char event_description[256];
    char source_module[64];
    uint8_t* additional_data;
    size_t data_size;
} audit_event_t;

typedef void (*audit_callback_t)(const audit_event_t* event);

// ============================================================================
// GESTIONNAIRE DE SÉCURITÉ
// ============================================================================

typedef struct {
    // Secure Boot
    secure_boot_config_t secure_boot;
    
    // Chiffrement
    fde_config_t fde_config;
    fbe_config_t* fbe_configs;
    size_t fbe_count;
    
    // Contrôle d'accès
    mac_policy_t mac_policy;
    
    // Sandboxing
    sandbox_config_t default_sandbox;
    
    // Audit
    audit_event_t* audit_log;
    size_t audit_log_size;
    size_t audit_log_count;
    audit_callback_t audit_callbacks[8];
    size_t callbacks_count;
    bool audit_to_disk;
    bool audit_to_remote;
    char remote_audit_server[256];
    
    // État
    bool initialized;
    bool security_enforced;
    uint64_t security_events_count;
    uint64_t last_security_scan;
} security_manager_t;

// ============================================================================
// API PUBLIQUE
// ============================================================================

/**
 * @brief Initialise le gestionnaire de sécurité
 * @param config Configuration initiale
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_manager_init(const security_manager_t* config);

/**
 * @brief Configure le Secure Boot
 * @param mode Mode de fonctionnement
 * @param keys Clés de signature
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_configure_secure_boot(secure_boot_mode_t mode, 
                                               const secure_boot_keys_t* keys);

/**
 * @brief Vérifie la signature d'un composant de boot
 * @param component_data Données du composant
 * @param component_size Taille des données
 * @param component_name Nom du composant
 * @return true si vérifié, false sinon
 */
bool security_verify_boot_component(const uint8_t* component_data, 
                                    size_t component_size,
                                    const char* component_name);

/**
 * @brief Active le chiffrement disque complet
 * @param config Configuration FDE
 * @param password Mot de passe utilisateur
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_enable_fde(const fde_config_t* config, 
                                   const char* password);

/**
 * @brief Active le chiffrement fichier par fichier
 * @param config Configuration FBE
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_enable_fbe(const fbe_config_t* config);

/**
 * @brief Déchiffre des données en place
 * @param data Données à déchiffrer
 * @param size Taille des données
 * @param key Clé de déchiffrement
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_decrypt_data(uint8_t* data, size_t size, 
                                     const uint8_t* key);

/**
 * @brief Chiffre des données en place
 * @param data Données à chiffrer
 * @param size Taille des données
 * @param key Clé de chiffrement
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_encrypt_data(uint8_t* data, size_t size, 
                                     const uint8_t* key);

/**
 * @brief Vérifie les permissions d'accès d'un processus
 * @param proc_ctx Contexte de sécurité du processus
 * @param res_ctx Contexte de sécurité de la ressource
 * @param requested_flags Flags demandés
 * @return true si autorisé, false sinon
 */
bool security_check_access(const process_security_ctx_t* proc_ctx,
                           const resource_security_ctx_t* res_ctx,
                           access_flags_t requested_flags);

/**
 * @brief Configure la politique MAC
 * @param policy Politique à appliquer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_set_mac_policy(const mac_policy_t* policy);

/**
 * @brief Applique un sandbox à un processus
 * @param pid PID du processus
 * @param config Configuration du sandbox
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_apply_sandbox(uint32_t pid, 
                                      const sandbox_config_t* config);

/**
 * @brief Enregistre un événement d'audit
 * @param event Événement à enregistrer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_log_audit_event(const audit_event_t* event);

/**
 * @brief Abonne un callback aux événements d'audit
 * @param callback Fonction de callback
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_register_audit_callback(audit_callback_t callback);

/**
 * @brief Génère une clé cryptographique sécurisée
 * @param key_buffer Buffer pour la clé
 * @param key_size Taille de la clé (bytes)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_generate_key(uint8_t* key_buffer, size_t key_size);

/**
 * @brief Scelle une clé avec le TPM
 * @param key Clé à sceller
 * @param key_size Taille de la clé
 * @param sealed_key Buffer pour la clé scellée
 * @param sealed_size Taille de la clé scellée
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_seal_key_with_tpm(const uint8_t* key, size_t key_size,
                                          uint8_t* sealed_key, size_t* sealed_size);

/**
 * @brief Déscelle une clé avec le TPM
 * @param sealed_key Clé scellée
 * @param sealed_size Taille de la clé scellée
 * @param key Buffer pour la clé déscellée
 * @param key_size Taille de la clé
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_unseal_key_with_tpm(const uint8_t* sealed_key, 
                                            size_t sealed_size,
                                            uint8_t* key, size_t* key_size);

/**
 * @brief Effectue un scan de sécurité complet
 * @param report_buffer Buffer pour le rapport
 * @param report_size Taille du buffer
 * @return Nombre de problèmes détectés
 */
size_t security_perform_full_scan(char* report_buffer, size_t report_size);

/**
 * @brief Arrête proprement le gestionnaire de sécurité
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t security_manager_shutdown(void);

#endif // AFROS_SECURITY_MANAGER_H
