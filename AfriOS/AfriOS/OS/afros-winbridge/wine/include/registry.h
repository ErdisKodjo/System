#ifndef AFROS_WINBRIDGE_REGISTRY_H
#define AFROS_WINBRIDGE_REGISTRY_H

/*
 * registry.h — API publique de l'émulateur de registre Win32 pour afros-winbridge.
 *
 * Déclare les hive handles prédéfinis, les codes d'accès et les fonctions
 * exposées par wine/registry (registry_emulator.c, hive_manager.c,
 * hive_io.c, system_hive.c, software_hive.c, sam_hive.c).
 */

#include "wine_compat.h"

/* --- Hive handles prédéfinis (pointeurs sentinelles) ------------------- */

#define HKEY_CLASSES_ROOT    ((HKEY)(ULONG_PTR)0x80000000)
#define HKEY_CURRENT_USER    ((HKEY)(ULONG_PTR)0x80000001)
#define HKEY_LOCAL_MACHINE   ((HKEY)(ULONG_PTR)0x80000002)
#define HKEY_USERS           ((HKEY)(ULONG_PTR)0x80000003)
#define HKEY_CURRENT_CONFIG  ((HKEY)(ULONG_PTR)0x80000005)

/* --- Droits d'accès ---------------------------------------------------- */

#define KEY_QUERY_VALUE         0x0001
#define KEY_SET_VALUE           0x0002
#define KEY_CREATE_SUB_KEY      0x0004
#define KEY_ENUMERATE_SUB_KEYS  0x0008
#define KEY_NOTIFY              0x0010
#define KEY_CREATE_LINK         0x0020
#define KEY_READ                0x20019
#define KEY_WRITE               0x20006
#define KEY_ALL_ACCESS          0xF003F

/* --- Codes de retour Reg* --------------------------------------------- */

#define ERROR_SUCCESS_REG       0L

/* --- Types de valeur registre ----------------------------------------- */

#define REG_NONE              0
#define REG_SZ                1
#define REG_EXPAND_SZ         2
#define REG_BINARY            3
#define REG_DWORD             4
#define REG_DWORD_BIG_ENDIAN  5
#define REG_LINK              6
#define REG_MULTI_SZ          7
#define REG_QWORD             11

/* --- Descripteur de clé ouverte --------------------------------------- */

typedef struct _REG_KEY {
    HKEY     hive;
    char     path[256];
    ULONG    access;
    ULONG    cur_index;
} REG_KEY, *PREG_KEY;

/* --- API registry_emulator.c ------------------------------------------ */

LONG RegOpenKey(HKEY hive, const char *subkey, REG_KEY **out);
LONG RegQueryValue(REG_KEY *key, const char *name, void *buf, DWORD *len);
LONG RegSetValue(REG_KEY *key, const char *name, ULONG type,
                 const void *data, DWORD len);
LONG RegCloseKey(REG_KEY *key);
LONG RegEnumKey(REG_KEY *key, DWORD index, char *name, DWORD name_max);

/* --- API hive_manager.c ----------------------------------------------- */

typedef struct _HIVE {
    HKEY     hive;
    char     name[16];
    char     file[256];
    BOOL     loaded;
    BOOL     dirty;
} HIVE, *PHIVE;

NTSTATUS HiveLoad(HKEY hive, const char *file);
NTSTATUS HiveSave(HKEY hive);
NTSTATUS HiveGetKey(HKEY hive, const char *path, REG_KEY **out);
NTSTATUS HiveManagerInit(void);
NTSTATUS HiveManagerShutdown(void);

/* --- API hive_io.c ---------------------------------------------------- */

typedef struct _HIVE_BIN_HEADER {
    DWORD signature;     /* "hbin" */
    DWORD offset;
    DWORD size;
    DWORD reserved[2];
    DWORD timestamp;
    DWORD spare;
} HIVE_BIN_HEADER;

NTSTATUS HiveReadHeader(const char *file, void *header_out);
NTSTATUS HiveReadCell(HANDLE hive_handle, ULONG cell_off, void *buf, ULONG *size);
NTSTATUS HiveWriteCell(HANDLE hive_handle, ULONG cell_off,
                       const void *data, ULONG size);

/* --- API system_hive.c ------------------------------------------------ */

NTSTATUS SystemHiveInit(void);
NTSTATUS SystemHiveLoadDriver(const char *driver_name);

/* --- API software_hive.c ---------------------------------------------- */

NTSTATUS SoftwareHiveInit(void);
NTSTATUS SoftwareHiveRegisterApp(const char *app_name, const char *exe_path);

/* --- API sam_hive.c --------------------------------------------------- */

NTSTATUS SamHiveInit(void);
NTSTATUS SamGetUserInfo(const char *user, char *sid_out, DWORD sid_len);

#endif /* AFROS_WINBRIDGE_REGISTRY_H */
