/**
 * @file vfs.h
 * @brief Système de fichiers virtuel (VFS) pour AfriOS
 */

#ifndef AFROS_VFS_H
#define AFROS_VFS_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/afros_types.h"

// Types de systèmes de fichiers supportés
typedef enum {
    FS_TYPE_UNKNOWN = 0,
    FS_TYPE_FAT32,
    FS_TYPE_FAT16,
    FS_TYPE_NTFS,
    FS_TYPE_EXT4,
    FS_TYPE_EXT3,
    FS_TYPE_APFS,
    FS_TYPE_BTRFS,
    FS_TYPE_ISO9660,
    FS_TYPE_UDF
} fs_type_t;

// Flags d'ouverture de fichier
#define VFS_O_READ      0x0001
#define VFS_O_WRITE     0x0002
#define VFS_O_APPEND    0x0004
#define VFS_O_CREATE    0x0008
#define VFS_O_TRUNCATE  0x0010
#define VFS_O_EXCL      0x0020

// Types de fichiers
typedef enum {
    VFS_FILE_TYPE_REGULAR = 0,
    VFS_FILE_TYPE_DIRECTORY,
    VFS_FILE_TYPE_SYMLINK,
    VFS_FILE_TYPE_BLOCK_DEVICE,
    VFS_FILE_TYPE_CHAR_DEVICE,
    VFS_FILE_TYPE_FIFO,
    VFS_FILE_TYPE_SOCKET
} vfs_file_type_t;

// Structure de nœud de fichier (inode-like)
typedef struct vfs_node {
    char name[256];
    vfs_file_type_t type;
    uint64_t size;
    uint64_t created_time;
    uint64_t modified_time;
    uint64_t accessed_time;
    uint32_t permissions;
    uint32_t uid;
    uint32_t gid;
    void* private_data;  // Données spécifiques au FS
    struct vfs_node* parent;
    struct vfs_node* children;
    struct vfs_node* next_sibling;
} vfs_node_t;

// Structure de descripteur de fichier
typedef struct vfs_file {
    uint32_t fd;
    vfs_node_t* node;
    uint64_t position;
    uint32_t flags;
    bool is_open;
    void* fs_private;  // Données privées du driver FS
} vfs_file_t;

// Opérations sur les systèmes de fichiers
typedef struct {
    const char* name;
    fs_type_t type;
    
    // Initialisation
    afros_status_t (*mount)(const char* device, vfs_node_t* root);
    afros_status_t (*unmount)(vfs_node_t* root);
    
    // Navigation
    afros_status_t (*open)(vfs_node_t* dir, const char* name, vfs_file_t* file);
    afros_status_t (*close)(vfs_file_t* file);
    afros_status_t (*read)(vfs_file_t* file, void* buffer, uint64_t size, uint64_t* bytes_read);
    afros_status_t (*write)(vfs_file_t* file, const void* buffer, uint64_t size, uint64_t* bytes_written);
    afros_status_t (*seek)(vfs_file_t* file, int64_t offset, int whence);
    
    // Directory operations
    afros_status_t (*readdir)(vfs_file_t* dir, vfs_node_t* entry);
    afros_status_t (*mkdir)(vfs_node_t* parent, const char* name);
    afros_status_t (*rmdir)(vfs_node_t* parent, const char* name);
    
    // File operations
    afros_status_t (*create)(vfs_node_t* parent, const char* name);
    afros_status_t (*unlink)(vfs_node_t* parent, const char* name);
    afros_status_t (*rename)(vfs_node_t* old_parent, const char* old_name, 
                             vfs_node_t* new_parent, const char* new_name);
    
    // Metadata
    afros_status_t (*stat)(vfs_node_t* node, vfs_node_t* out_node);
    afros_status_t (*chmod)(vfs_node_t* node, uint32_t permissions);
    afros_status_t (*chown)(vfs_node_t* node, uint32_t uid, uint32_t gid);
    
    // Sync
    afros_status_t (*sync)(vfs_node_t* root);
    afros_status_t (*fsync)(vfs_file_t* file);
} fs_driver_t;

// Structure du VFS
typedef struct {
    vfs_node_t* root;
    vfs_file_t* files;
    uint32_t max_files;
    uint32_t next_fd;
    fs_driver_t** mounted_fs;
    uint32_t mount_count;
} vfs_t;

// Fonctions principales du VFS
afros_status_t vfs_init(void);
afros_status_t vfs_mount(const char* device_path, const char* mount_point, fs_type_t fs_type);
afros_status_t vfs_unmount(const char* mount_point);
afros_status_t vfs_open(const char* path, uint32_t flags, uint32_t* fd);
afros_status_t vfs_close(uint32_t fd);
afros_status_t vfs_read(uint32_t fd, void* buffer, uint64_t size, uint64_t* bytes_read);
afros_status_t vfs_write(uint32_t fd, const void* buffer, uint64_t size, uint64_t* bytes_written);
afros_status_t vfs_seek(uint32_t fd, int64_t offset, int whence);
afros_status_t vfs_readdir(uint32_t fd, char* name, uint64_t* size, vfs_file_type_t* type);
afros_status_t vfs_mkdir(const char* path);
afros_status_t vfs_remove(const char* path);
afros_status_t vfs_rename(const char* old_path, const char* new_path);
afros_status_t vfs_stat(const char* path, vfs_node_t* stat_buf);

// Enregistrement des drivers
afros_status_t vfs_register_driver(fs_driver_t* driver);
afros_status_t vfs_unregister_driver(fs_type_t type);

// Utilitaires
afros_status_t vfs_detect_fs_type(const char* device_path, fs_type_t* detected_type);
const char* vfs_get_type_name(fs_type_t type);
bool vfs_is_path_absolute(const char* path);
afros_status_t vfs_resolve_path(const char* path, char* resolved, uint32_t max_len);

#endif // AFROS_VFS_H
