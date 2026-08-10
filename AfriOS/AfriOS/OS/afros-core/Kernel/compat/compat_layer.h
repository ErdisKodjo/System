/**
 * @file compat_layer.h
 * @brief Couches de compatibilité pour applications multi-OS
 */

#ifndef AFROS_COMPAT_LAYER_H
#define AFROS_COMPAT_LAYER_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/afros_types.h"

// Types de systèmes compatibles
typedef enum {
    COMPAT_NATIVE = 0,      // Applications natives AfriOS (ELF x86_64)
    COMPAT_LINUX,           // Binaires Linux (ELF)
    COMPAT_ANDROID,         // Applications Android (APK/Dex)
    COMPAT_WINDOWS_PE,      // Applications Windows (PE32+)
    COMPAT_POSIX,           // Applications POSIX standard
} compat_type_t;

// Structure d'un processus compatible
typedef struct {
    uint32_t pid;
    compat_type_t type;
    char name[128];
    void* image_base;
    uint64_t image_size;
    void* stack_base;
    uint64_t stack_size;
    void* heap_base;
    uint64_t heap_size;
    void* entry_point;
    bool is_running;
    int exit_code;
    void* compat_context;  // Contexte spécifique à la couche
} compat_process_t;

// ============================================================================
// COUCHE DE COMPATIBILITÉ LINUX (Linux ABI)
// ============================================================================

// Syscalls Linux supportés
typedef enum {
    LINUX_SYS_read = 0,
    LINUX_SYS_write = 1,
    LINUX_SYS_open = 2,
    LINUX_SYS_close = 3,
    LINUX_SYS_stat = 4,
    LINUX_SYS_fstat = 5,
    LINUX_SYS_mmap = 9,
    LINUX_SYS_mprotect = 10,
    LINUX_SYS_munmap = 11,
    LINUX_SYS_brk = 12,
    LINUX_SYS_execve = 59,
    LINUX_SYS_exit = 60,
    LINUX_SYS_fork = 57,
    LINUX_SYS_clone = 56,
    LINUX_SYS_getpid = 39,
    LINUX_SYS_getuid = 102,
    LINUX_SYS_socket = 41,
    LINUX_SYS_connect = 42,
    LINUX_SYS_accept = 43,
    LINUX_SYS_sendto = 44,
    LINUX_SYS_recvfrom = 45,
} linux_syscall_num_t;

// Structures Linux
typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    int64_t st_atime;
    int64_t st_mtime;
    int64_t st_ctime;
} linux_stat_t;

// Fonctions de la couche Linux
afros_status_t linux_compat_init(void);
afros_status_t linux_load_binary(const char* path, compat_process_t* process);
afros_status_t linux_execute(compat_process_t* process);
int64_t linux_handle_syscall(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                             uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);
afros_status_t linux_setup_process(compat_process_t* process);
afros_status_t linux_teardown_process(compat_process_t* process);

// Traduction ELF Linux -> AfriOS
afros_status_t linux_parse_elf(const void* elf_data, compat_process_t* process);
afros_status_t linux_map_segments(compat_process_t* process);
afros_status_t linux_setup_vdso(compat_process_t* process);

// ============================================================================
// COUCHE DE COMPATIBILITÉ ANDROID (ART/Dalvik)
// ============================================================================

// Types d'applications Android
typedef enum {
    ANDROID_APP_NATIVE = 0,   // NDK (C/C++)
    ANDROID_APP_JAVA,         // Java/Kotlin (Dex)
    ANDROID_APP_HYBRID        // Mixte
} android_app_type_t;

// Contexte d'exécution Android
typedef struct {
    android_app_type_t app_type;
    void* dex_data;
    uint64_t dex_size;
    void* native_libs;
    uint32_t native_lib_count;
    void* art_runtime;
    void* jni_env;
    uint32_t sdk_version;
    char package_name[128];
} android_compat_context_t;

// Fonctions de la couche Android
afros_status_t android_compat_init(void);
afros_status_t android_load_apk(const char* apk_path, compat_process_t* process);
afros_status_t android_install_apk(const char* apk_path);
afros_status_t android_uninstall_apk(const char* package_name);
afros_status_t android_launch_app(const char* package_name, const char* activity);
afros_status_t android_stop_app(const char* package_name);

// Runtime ART
afros_status_t android_init_art_runtime(android_compat_context_t* ctx);
afros_status_t android_load_dex(android_compat_context_t* ctx, const void* dex_data);
afros_status_t android_execute_dex(android_compat_context_t* ctx, const char* class_name, 
                                   const char* method_name);

// JNI Bridge
afros_status_t android_setup_jni(android_compat_context_t* ctx);
afros_status_t android_call_java_method(android_compat_context_t* ctx, const char* signature, ...);

// Services Android
afros_status_t android_start_surface_flinger(void);
afros_status_t android_start_audio_flinger(void);
afros_status_t android_start_media_server(void);

// ============================================================================
// COUCHE DE COMPATIBILITÉ WINDOWS (PE32+)
// ============================================================================

// Structures PE
typedef struct {
    uint16_t dos_magic;
    uint16_t pe_magic;
    uint16_t machine;
    uint16_t num_sections;
    uint32_t timestamp;
    uint32_t characteristics;
    uint16_t optional_header_magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t entry_point_rva;
    uint32_t base_of_code;
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t subsystem;
    uint32_t dll_characteristics;
    uint64_t size_of_stack_reserve;
    uint64_t size_of_stack_commit;
    uint64_t size_of_heap_reserve;
    uint64_t size_of_heap_commit;
} pe_header_t;

// Appels système Windows (NT syscalls)
typedef enum {
    WIN_SYS_NtCreateFile = 0x55,
    WIN_SYS_NtReadFile = 0x03,
    WIN_SYS_NtWriteFile = 0x08,
    WIN_SYS_NtClose = 0x0F,
    WIN_SYS_NtCreateProcess = 0xA5,
    WIN_SYS_NtCreateThread = 0xA1,
    WIN_SYS_NtTerminateProcess = 0x2B,
    WIN_SYS_NtAllocateVirtualMemory = 0x18,
    WIN_SYS_NtFreeVirtualMemory = 0x1E,
    WIN_SYS_NtQueryInformationProcess = 0x19,
} win_syscall_num_t;

// Contexte d'exécution Windows
typedef struct {
    pe_header_t pe_header;
    void* pe_image;
    uint64_t pe_image_size;
    void* peb;  // Process Environment Block
    void* teb;  // Thread Environment Block
    void* ntdll_handle;
    void** import_table;
    uint32_t import_count;
} windows_compat_context_t;

// Fonctions de la couche Windows
afros_status_t windows_compat_init(void);
afros_status_t windows_load_pe(const char* path, compat_process_t* process);
afros_status_t windows_execute_pe(compat_process_t* process);
int64_t windows_handle_syscall(uint64_t syscall_num, uint64_t* args);

// Parsing PE
afros_status_t windows_parse_pe(const void* pe_data, compat_process_t* process);
afros_status_t windows_relocate_pe(compat_process_t* process, uint64_t new_base);
afros_status_t windows_resolve_imports(compat_process_t* process);

// Win32 API Emulation
afros_status_t windows_emulate_createfile(windows_compat_context_t* ctx, void* args);
afros_status_t windows_emulate_readfile(windows_compat_context_t* ctx, void* args);
afros_status_t windows_emulate_writefile(windows_compat_context_t* ctx, void* args);
afros_status_t windows_emulate_virtualalloc(windows_compat_context_t* ctx, void* args);
afros_status_t windows_emulate_createprocess(windows_compat_context_t* ctx, void* args);
afros_status_t windows_emulate_createthread(windows_compat_context_t* ctx, void* args);

// ============================================================================
// COUCHE DE COMPATIBILITÉ POSIX
// ============================================================================

// Fonctions POSIX standard
afros_status_t posix_compat_init(void);
afros_status_t posix_spawn(const char* path, const char* argv[], const char* envp[]);
afros_status_t posix_fork(uint32_t* child_pid);
afros_status_t posix_exec(const char* path, const char* argv[], const char* envp[]);
void posix_exit(int status);
afros_status_t posix_pipe(int pipefd[2]);
afros_status_t posix_dup2(int oldfd, int newfd);

// Threads POSIX (pthreads)
afros_status_t posix_pthread_create(void* thread, const void* attr, 
                                    void* (*start_routine)(void*), void* arg);
afros_status_t posix_pthread_join(void* thread, void** retval);
afros_status_t posix_pthread_mutex_lock(void* mutex);
afros_status_t posix_pthread_mutex_unlock(void* mutex);

// ============================================================================
// GESTIONNAIRE DE COMPATIBILITÉ UNIFIÉ
// ============================================================================

// Initialise toutes les couches de compatibilité
afros_status_t compat_layer_init_all(void);

// Charge et exécute un binaire de manière transparente
afros_status_t compat_execute(const char* path, const char* args[], const char* envp[]);

// Détecte automatiquement le type de binaire
afros_status_t compat_detect_type(const char* path, compat_type_t* detected_type);

// Crée un processus compatible
afros_status_t compat_create_process(const char* path, compat_process_t** process);

// Détruit un processus compatible
afros_status_t compat_destroy_process(compat_process_t* process);

// Liste les processus compatibles en cours d'exécution
afros_status_t compat_list_processes(compat_process_t** processes, uint32_t* count);

#endif // AFROS_COMPAT_LAYER_H
