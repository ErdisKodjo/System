/*
 * system_hive.c — Populate HKLM\System au boot.
 *
 * Crée une structure de registre système minimale: liste de pilotes,
 * services, paramètres de contrôle Windows. Utilisé au démarrage de
 * wineserver pour initialiser les clés System\\CurrentControlSet\\Services.
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <string.h>

/* Pilotes et services système à enregistrer par défaut. */
static const char *g_default_drivers[] = {
    "ACPI",
    "Disk",
    "Tcpip",
    "NDIS",
    "Beep",
    "Null",
    "VgaSave",
    "afros-hal",
    "afros-vfs",
    "afros-net",
    "afros-gpu",
};

#define G_DEFAULT_DRIVER_COUNT \
    (sizeof(g_default_drivers) / sizeof(g_default_drivers[0]))

/* Liste des services système. */
static const char *g_default_services[] = {
    "EventLog",
    "PlugPlay",
    "Winmgmt",
    "Schedule",
    "Spooler",
    "LanmanServer",
    "LanmanWorkstation",
};

#define G_DEFAULT_SERVICE_COUNT \
    (sizeof(g_default_services) / sizeof(g_default_services[0]))

/* --- API publique ------------------------------------------------------ */

/* Initialise le hive System avec les entrées par défaut. */
NTSTATUS SystemHiveInit(void)
{
    REG_KEY *root;
    NTSTATUS s;
    DWORD i;

    s = HiveGetKey(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet", &root);
    if (!NT_SUCCESS(s)) return s;
    RegCloseKey(root);

    for (i = 0; i < G_DEFAULT_DRIVER_COUNT; i++) {
        SystemHiveLoadDriver(g_default_drivers[i]);
    }
    for (i = 0; i < G_DEFAULT_SERVICE_COUNT; i++) {
        char path[256];
        REG_KEY *svc;
        DWORD start_type = 2; /* SERVICE_AUTO_START */
        snprintf(path, sizeof(path),
                 "System\\CurrentControlSet\\Services\\%s",
                 g_default_services[i]);
        if (NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &svc))) {
            RegSetValue(svc, "Start", REG_DWORD, &start_type, sizeof(start_type));
            RegCloseKey(svc);
        }
    }
    return STATUS_SUCCESS;
}

/* Enregistre un pilote dans le hive System. */
NTSTATUS SystemHiveLoadDriver(const char *driver_name)
{
    char path[256];
    REG_KEY *drv;
    DWORD start_type  = 1;  /* SERVICE_SYSTEM_START */
    DWORD error_ctl   = 1;  /* SERVICE_ERROR_NORMAL */
    DWORD driver_type = 1;  /* SERVICE_KERNEL_DRIVER */

    if (!driver_name) return STATUS_INVALID_PARAMETER;
    snprintf(path, sizeof(path),
             "System\\CurrentControlSet\\Services\\%s", driver_name);
    if (!NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &drv)))
        return STATUS_UNSUCCESSFUL;
    RegSetValue(drv, "Start",      REG_DWORD, &start_type,  sizeof(start_type));
    RegSetValue(drv, "ErrorControl", REG_DWORD, &error_ctl, sizeof(error_ctl));
    RegSetValue(drv, "Type",       REG_DWORD, &driver_type, sizeof(driver_type));
    RegCloseKey(drv);
    return STATUS_SUCCESS;
}

/* Marque un service pour démarrage différé. */
NTSTATUS SystemHiveDelayStart(const char *service_name)
{
    char path[256];
    REG_KEY *svc;
    DWORD start_type = 3; /* SERVICE_DEMAND_START */
    if (!service_name) return STATUS_INVALID_PARAMETER;
    snprintf(path, sizeof(path),
             "System\\CurrentControlSet\\Services\\%s", service_name);
    if (!NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &svc)))
        return STATUS_NOT_FOUND;
    RegSetValue(svc, "Start", REG_DWORD, &start_type, sizeof(start_type));
    RegCloseKey(svc);
    return STATUS_SUCCESS;
}
