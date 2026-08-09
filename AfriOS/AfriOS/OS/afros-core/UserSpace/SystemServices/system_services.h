/**
 * @file system_services.h
 * @brief Services système essentiels pour AfriOS
 * 
 * Fournit les services utilisateurspace incluant :
 * - Gestion des processus et threads
 * - IPC (pipes, sockets, shared memory, messages)
 * - Système de fichiers virtuel utilisateur
 * - Gestion de l'énergie utilisateur
 * - Logging système unifié
 * - Timekeeping et timers
 */

#ifndef AFROS_SYSTEM_SERVICES_H
#define AFROS_SYSTEM_SERVICES_H

#include <stdint.h>
#include <stdbool.h>
#include "afros_types.h"

// ============================================================================
// GESTION DES PROCESSUS
// ============================================================================

typedef enum {
    PROCESS_STATE_IDLE = 0,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_SLEEPING,
    PROCESS_STATE_STOPPED,
    PROCESS_STATE_ZOMBIE,
    PROCESS_STATE_DEAD
} process_state_t;

typedef enum {
    PRIORITY_REALTIME = 0,    // 0-99
    PRIORITY_HIGH = 100,      // 100-129
    PRIORITY_NORMAL = 130,    // 130-139
    PRIORITY_LOW = 140,       // 140-149
    PRIORITY_IDLE = 150       // 150-255
} process_priority_t;

typedef struct {
    uint32_t pid;
    uint32_t ppid;            // parent PID
    uint32_t tid;             // thread ID principal
    char name[64];
    char cmdline[256];
    char cwd[256];            // current working directory
    process_state_t state;
    process_priority_t priority;
    uint32_t uid;
    uint32_t gid;
    uint32_t* supplementary_gids;
    size_t gids_count;
    uint64_t start_time;
    uint64_t cpu_time_user;
    uint64_t cpu_time_kernel;
    uint64_t memory_rss;      // resident set size
    uint64_t memory_vms;      // virtual memory size
    uint32_t num_threads;
    uint32_t num_fds;
    uint32_t num_children;
    int exit_code;
    bool foreground;
} process_info_t;

typedef struct {
    uint32_t tid;
    uint32_t pid;
    char name[64];
    process_state_t state;
    uint64_t cpu_time;
    uint64_t wait_time;
    uint32_t core_affinity;   // bitmask des cœurs
    void* stack_base;
    size_t stack_size;
    void* tls_base;           // thread-local storage
    int sched_policy;
    int nice_value;
} thread_info_t;

// ============================================================================
// IPC (INTER-PROCESS COMMUNICATION)
// ============================================================================

typedef enum {
    IPC_PIPE = 0,
    IPC_FIFO,
    IPC_MESSAGE_QUEUE,
    IPC_SHARED_MEMORY,
    IPC_SOCKET_UNIX,
    IPC_SEMAPHORE,
    IPC_MUTEX,
    IPC_CONDITION_VARIABLE
} ipc_type_t;

typedef struct {
    int read_fd;
    int write_fd;
    size_t capacity;
    size_t used;
    bool non_blocking;
} pipe_t;

typedef struct {
    char name[64];
    int mq_id;
    size_t max_messages;
    size_t message_size;
    size_t current_messages;
    bool blocking;
} message_queue_t;

typedef struct {
    char name[64];
    int shm_id;
    void* address;
    size_t size;
    bool read_only;
    bool inherited;
} shared_memory_t;

typedef struct {
    char name[64];
    int sem_id;
    uint32_t value;
    uint32_t max_value;
    bool process_shared;
} semaphore_t;

typedef struct {
    char name[64];
    int mutex_id;
    int type;                 // normal, recursive, errorcheck
    bool process_shared;
    bool robust;
} ipc_mutex_t;

typedef struct {
    char name[64];
    int cond_id;
    int clock_id;             // CLOCK_REALTIME ou CLOCK_MONOTONIC
} condition_variable_t;

// ============================================================================
// SIGNAUX
// ============================================================================

#define SIGNAL_COUNT 64

typedef enum {
    SIGNULL = 0,
    SIGHUP = 1,
    SIGINT = 2,
    SIGQUIT = 3,
    SIGILL = 4,
    SIGTRAP = 5,
    SIGABRT = 6,
    SIGBUS = 7,
    SIGFPE = 8,
    SIGKILL = 9,
    SIGUSR1 = 10,
    SIGSEGV = 11,
    SIGUSR2 = 12,
    SIGPIPE = 13,
    SIGALRM = 14,
    SIGTERM = 15,
    SIGCHLD = 17,
    SIGCONT = 18,
    SIGSTOP = 19,
    SIGTSTP = 20,
    SIGTTIN = 21,
    SIGTTOU = 22,
    SIGURG = 23,
    SIGXCPU = 24,
    SIGXFSZ = 25,
    SIGVTALRM = 26,
    SIGPROF = 27,
    SIGWINCH = 28,
    SIGIO = 29,
    SIGPOLL = SIGIO,
    SIGSYS = 31,
    SIGRTMIN = 34,
    SIGRTMAX = 64
} signal_number_t;

typedef enum {
    SIGNAL_ACTION_DEFAULT = 0,
    SIGNAL_ACTION_IGNORE,
    SIGNAL_ACTION_HANDLER,
    SIGNAL_ACTION_HANDLER_SIGINFO
} signal_action_type_t;

typedef void (*signal_handler_t)(int signum);
typedef void (*signal_handler_siginfo_t)(int signum, siginfo_t* info, void* context);

typedef struct {
    signal_action_type_t action_type;
    union {
        signal_handler_t handler;
        signal_handler_siginfo_t handler_siginfo;
    } handler;
    uint32_t mask;            // signaux masqués pendant l'exécution
    uint32_t flags;           // SA_NOCLDSTOP, SA_RESTART, etc.
} signal_action_t;

typedef struct {
    uint32_t pending;         // bitmask des signaux en attente
    uint32_t blocked;         // bitmask des signaux bloqués
    signal_action_t actions[SIGNAL_COUNT];
} signal_context_t;

// ============================================================================
// LOGGING SYSTÈME
// ============================================================================

typedef enum {
    LOG_LEVEL_EMERG = 0,      // Système inutilisable
    LOG_LEVEL_ALERT,          // Action immédiate requise
    LOG_LEVEL_CRIT,           // Conditions critiques
    LOG_LEVEL_ERR,            // Erreurs
    LOG_LEVEL_WARNING,        // Avertissements
    LOG_LEVEL_NOTICE,         // Normal mais significatif
    LOG_LEVEL_INFO,           // Informations
    LOG_LEVEL_DEBUG           // Debug
} log_level_t;

typedef enum {
    LOG_FACILITY_KERN = 0,
    LOG_FACILITY_USER,
    LOG_FACILITY_MAIL,
    LOG_FACILITY_DAEMON,
    LOG_FACILITY_AUTH,
    LOG_FACILITY_SYSLOG,
    LOG_FACILITY_LPR,
    LOG_FACILITY_NEWS,
    LOG_FACILITY_UUCP,
    LOG_FACILITY_LOCAL0,
    LOG_FACILITY_LOCAL1,
    LOG_FACILITY_LOCAL2,
    LOG_FACILITY_LOCAL3,
    LOG_FACILITY_LOCAL4,
    LOG_FACILITY_LOCAL5,
    LOG_FACILITY_LOCAL6,
    LOG_FACILITY_LOCAL7
} log_facility_t;

typedef struct {
    log_level_t level;
    log_facility_t facility;
    uint64_t timestamp;
    uint32_t pid;
    uint32_t tid;
    char tag[32];
    char message[512];
    char source_file[64];
    uint32_t source_line;
} log_entry_t;

typedef void (*log_callback_t)(const log_entry_t* entry);

typedef struct {
    bool enabled;
    log_level_t min_level;
    bool log_to_console;
    bool log_to_file;
    char log_file_path[256];
    uint64_t max_file_size;   // rotation
    uint32_t max_files;       // nombre de fichiers retenus
    bool log_to_socket;       // /dev/log
    log_callback_t callback;
    uint64_t entries_count;
    uint64_t dropped_count;
} logging_config_t;

// ============================================================================
// TIMEKEEPING ET TIMERS
// ============================================================================

typedef enum {
    CLOCK_REALTIME = 0,
    CLOCK_MONOTONIC,
    CLOCK_BOOTTIME,
    CLOCK_TAI,
    CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_THREAD_CPUTIME_ID
} clock_id_t;

typedef struct {
    int64_t tv_sec;
    int32_t tv_nsec;
} timespec_t;

typedef enum {
    TIMER_TYPE_ONESHOT = 0,
    TIMER_TYPE_PERIODIC,
    TIMER_TYPE_RELATIVE,
    TIMER_TYPE_ABSOLUTE
} timer_type_t;

typedef void (*timer_callback_t)(uint32_t timer_id, void* user_data);

typedef struct {
    uint32_t timer_id;
    timer_type_t type;
    clock_id_t clock_id;
    timespec_t interval;
    timespec_t next_expiry;
    timer_callback_t callback;
    void* user_data;
    bool active;
    bool overrun;
    uint32_t overruns_count;
} system_timer_t;

typedef struct {
    clock_id_t clock_id;
    bool synchronized;
    char ntp_server[256];
    uint64_t last_sync;
    int64_t offset_ns;        // offset par rapport au temps réel
    int64_t drift_ns_per_sec; // dérive estimée
} time_sync_config_t;

// ============================================================================
// GESTION DE L'ÉNERGIE UTILISATEUR
// ============================================================================

typedef enum {
    POWER_STATE_ACTIVE = 0,
    POWER_STATE_SCREEN_OFF,
    POWER_STATE_SUSPEND,
    POWER_STATE_HIBERNATE,
    POWER_STATE_SHUTDOWN
} power_state_t;

typedef enum {
    WAKE_SOURCE_NONE = 0,
    WAKE_SOURCE_POWER_BUTTON,
    WAKE_SOURCE_LID_OPEN,
    WAKE_SOURCE_RTC_ALARM,
    WAKE_SOURCE_USB,
    WAKE_SOURCE_NETWORK,
    WAKE_SOURCE_KEYBOARD,
    WAKE_SOURCE_MOUSE
} wake_source_t;

typedef struct {
    power_state_t current_state;
    power_state_t target_state;
    uint32_t battery_percent;
    bool battery_charging;
    uint32_t battery_time_remaining;  // minutes
    bool ac_present;
    uint32_t thermal_zone_temp;       // Celsius * 100
    uint32_t cpu_frequency_mhz;
    uint32_t cpu_governor;            // performance, powersave, etc.
    uint64_t uptime_seconds;
    uint64_t suspend_count;
    wake_source_t last_wake_source;
} power_status_t;

typedef void (*power_state_callback_t)(power_state_t old_state, 
                                        power_state_t new_state);

// ============================================================================
// VARIABLES D'ENVIRONNEMENT
// ============================================================================

typedef struct {
    char name[256];
    char value[1024];
    bool exported;
} environment_variable_t;

typedef struct {
    environment_variable_t* variables;
    size_t count;
    size_t capacity;
} environment_block_t;

// ============================================================================
// LIMITES ET RESSOURCES
// ============================================================================

typedef enum {
    RLIMIT_CPU = 0,
    RLIMIT_FSIZE,
    RLIMIT_DATA,
    RLIMIT_STACK,
    RLIMIT_CORE,
    RLIMIT_RSS,
    RLIMIT_NPROC,
    RLIMIT_NOFILE,
    RLIMIT_MEMLOCK,
    RLIMIT_AS,
    RLIMIT_LOCKS,
    RLIMIT_SIGPENDING,
    RLIMIT_MSGQUEUE,
    RLIMIT_NICE,
    RLIMIT_RTPRIO,
    RLIMIT_RTTEIME,
    RLIMIT_COUNT
} resource_limit_type_t;

typedef struct {
    uint64_t soft_limit;
    uint64_t hard_limit;
} resource_limit_t;

typedef struct {
    resource_limit_t limits[RLIMIT_COUNT];
    uint64_t open_files;
    uint64_t threads;
    uint64_t processes;
    uint64_t locked_memory;
    uint64_t queued_signals;
    uint64_t msgqueue_bytes;
} resource_usage_t;

// ============================================================================
// MANAGER DE SERVICES SYSTÈME
// ============================================================================

typedef struct {
    // Processus
    process_info_t* processes;
    size_t process_count;
    uint32_t init_pid;
    
    // Threads
    thread_info_t* threads;
    size_t thread_count;
    
    // IPC
    pipe_t pipes[1024];
    message_queue_t message_queues[256];
    shared_memory_t shared_memories[256];
    semaphore_t semaphores[256];
    ipc_mutex_t mutexes[256];
    condition_variable_t cond_vars[256];
    
    // Signaux
    signal_context_t global_signal_ctx;
    
    // Logging
    logging_config_t logging;
    
    // Timers
    system_timer_t timers[256];
    size_t timer_count;
    time_sync_config_t time_sync;
    
    // Power
    power_status_t power;
    power_state_callback_t power_callbacks[8];
    size_t power_callback_count;
    
    // Environment
    environment_block_t global_env;
    
    // Ressources
    resource_usage_t system_resources;
    
    // État
    bool initialized;
    uint64_t boot_time;
    char hostname[64];
    char domainname[64];
} system_services_manager_t;

// ============================================================================
// API PUBLIQUE
// ============================================================================

/**
 * @brief Initialise le manager de services système
 * @param config Configuration initiale
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_services_init(const system_services_manager_t* config);

/**
 * @brief Crée un nouveau processus
 * @param executable Chemin de l'exécutable
 * @param argv Arguments
 * @param envp Environnement
 * @param info Info du processus créé (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_create_process(const char* executable, 
                                      const char** argv, 
                                      const char** envp,
                                      process_info_t* info);

/**
 * @brief Récupère les informations d'un processus
 * @param pid PID du processus
 * @param info Structure à remplir (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_get_process_info(uint32_t pid, process_info_t* info);

/**
 * @brief Liste tous les processus
 * @param pids Tableau de PIDs (sortie)
 * @param max_pids Taille maximale du tableau
 * @return Nombre de processus listés
 */
size_t system_list_processes(uint32_t* pids, size_t max_pids);

/**
 * @brief Termine un processus
 * @param pid PID du processus
 * @param signal Signal à envoyer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_terminate_process(uint32_t pid, int signal);

/**
 * @brief Crée un pipe
 * @param pipe Structure pipe à initialiser
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_create_pipe(pipe_t* pipe);

/**
 * @brief Crée une file de messages
 * @param name Nom de la file
 * @param max_messages Nombre maximal de messages
 * @param message_size Taille maximale d'un message
 * @param mq File de messages (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_create_message_queue(const char* name,
                                            size_t max_messages,
                                            size_t message_size,
                                            message_queue_t* mq);

/**
 * @brief Crée une mémoire partagée
 * @param name Nom de la mémoire partagée
 * @param size Taille en bytes
 * @param shm Mémoire partagée (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_create_shared_memory(const char* name, size_t size,
                                            shared_memory_t* shm);

/**
 * @brief Crée un sémaphore
 * @param name Nom du sémaphore
 * @param initial_value Valeur initiale
 * @param sem Sémaphore (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_create_semaphore(const char* name, uint32_t initial_value,
                                        semaphore_t* sem);

/**
 * @brief Attend un sémaphore
 * @param sem Sémaphore
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_semaphore_wait(semaphore_t* sem);

/**
 * @brief Signale un sémaphore
 * @param sem Sémaphore
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_semaphore_post(semaphore_t* sem);

/**
 * @brief Envoie un signal à un processus
 * @param pid PID du processus
 * @param signum Numéro du signal
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_send_signal(uint32_t pid, int signum);

/**
 * @brief Configure un handler de signal
 * @param signum Numéro du signal
 * @param action Action à configurer
 * @param old_action Ancienne action (sortie, optionnel)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_configure_signal(int signum, const signal_action_t* action,
                                        signal_action_t* old_action);

/**
 * @brief Log un message
 * @param level Niveau de log
 * @param tag Tag du message
 * @param format Format du message (printf-style)
 * @param ... Arguments
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_log(log_level_t level, const char* tag, 
                           const char* format, ...);

/**
 * @brief Crée un timer
 * @param type Type de timer
 * @param interval Intervalle (oneshot ou period)
 * @param callback Callback d'expiration
 * @param user_data Données utilisateur
 * @param timer_id ID du timer créé (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_create_timer(timer_type_t type, 
                                    const timespec_t* interval,
                                    timer_callback_t callback,
                                    void* user_data,
                                    uint32_t* timer_id);

/**
 * @brief Détruit un timer
 * @param timer_id ID du timer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_destroy_timer(uint32_t timer_id);

/**
 * @brief Récupère l'heure actuelle
 * @param clock_id Horloge à utiliser
 * @param ts Timespec à remplir (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_get_time(clock_id_t clock_id, timespec_t* ts);

/**
 * @brief Définit l'heure
 * @param clock_id Horloge à modifier
 * @param ts Nouvelle heure
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_set_time(clock_id_t clock_id, const timespec_t* ts);

/**
 * @brief Synchronise l'heure avec NTP
 * @param ntp_server Serveur NTP
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_sync_time_ntp(const char* ntp_server);

/**
 * @brief Récupère le statut énergétique
 * @param status Structure de statut (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_get_power_status(power_status_t* status);

/**
 * @brief Demande un changement d'état énergétique
 * @param state État cible
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_request_power_state(power_state_t state);

/**
 * @brief Configure une variable d'environnement
 * @param name Nom de la variable
 * @param value Valeur
 * @param exported true pour exporter aux enfants
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_set_environment(const char* name, const char* value,
                                       bool exported);

/**
 * @brief Récupère une variable d'environnement
 * @param name Nom de la variable
 * @return Valeur ou NULL si non trouvée
 */
const char* system_get_environment(const char* name);

/**
 * @brief Récupère l'utilisation des ressources
 * @param pid PID du processus (0 pour système entier)
 * @param usage Structure d'utilisation (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_get_resource_usage(uint32_t pid, 
                                          resource_usage_t* usage);

/**
 * @brief Définit une limite de ressource
 * @param pid PID du processus
 * @param type Type de limite
 * @param limit Nouvelle limite
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_set_resource_limit(uint32_t pid,
                                          resource_limit_type_t type,
                                          const resource_limit_t* limit);

/**
 * @brief Nettoie le manager de services
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t system_services_shutdown(void);

#endif // AFROS_SYSTEM_SERVICES_H
