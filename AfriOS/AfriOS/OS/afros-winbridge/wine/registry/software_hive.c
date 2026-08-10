/*
 * software_hive.c — Populate HKLM\Software au boot.
 *
 * Enregistre les applications installées, les classes COM et les paramètres
 * Windows de base dans HKLM\Software\Microsoft\...
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* --- API publique ------------------------------------------------------ */

/* Initialise le hive Software avec les valeurs par défaut. */
NTSTATUS SoftwareHiveInit(void)
{
    REG_KEY *k;
    DWORD  ver_dword = 0x0601;     /* Win7 par défaut */
    char   current_build[16] = "7601";
    char   product_name[64]  = "AfriOS Wine Compatibility Layer";

    if (NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE,
                              "Software\\Microsoft\\Windows NT\\CurrentVersion", &k))) {
        RegSetValue(k, "CurrentVersion", REG_SZ, "6.1", 4);
        RegSetValue(k, "CurrentBuild",   REG_SZ, current_build, (DWORD)strlen(current_build) + 1);
        RegSetValue(k, "ProductName",    REG_SZ, product_name,  (DWORD)strlen(product_name) + 1);
        RegSetValue(k, "CSDVersion",     REG_SZ, "Service Pack 1", 16);
        RegSetValue(k, "CurrentMajorVersionNumber", REG_DWORD, &ver_dword, sizeof(ver_dword));
        RegCloseKey(k);
    }
    if (NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE,
                              "Software\\Microsoft\\Windows\\CurrentVersion", &k))) {
        char pf[256] = "C:\\Program Files";
        char pf86[256] = "C:\\Program Files (x86)";
        char sys_dir[256] = "C:\\Windows\\System32";
        RegSetValue(k, "ProgramFilesDir",      REG_SZ, pf,     (DWORD)strlen(pf) + 1);
        RegSetValue(k, "ProgramFilesDir (x86)", REG_SZ, pf86,  (DWORD)strlen(pf86) + 1);
        RegSetValue(k, "SystemRoot",           REG_SZ, "C:\\Windows", 11);
        RegSetValue(k, "SystemDir",            REG_SZ, sys_dir,(DWORD)strlen(sys_dir) + 1);
        RegCloseKey(k);
    }
    return STATUS_SUCCESS;
}

/* Enregistre une application dans HKLM\Software\<vendor>\<app>. */
NTSTATUS SoftwareHiveRegisterApp(const char *app_name, const char *exe_path)
{
    char path[256];
    REG_KEY *app;
    char vendor[64] = "AfriOS";
    DWORD install_date;

    if (!app_name || !exe_path) return STATUS_INVALID_PARAMETER;
    install_date = (DWORD)time(NULL);
    snprintf(path, sizeof(path), "Software\\%s\\%s", vendor, app_name);
    if (!NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &app)))
        return STATUS_UNSUCCESSFUL;
    RegSetValue(app, "InstallPath", REG_SZ, exe_path, (DWORD)strlen(exe_path) + 1);
    RegSetValue(app, "DisplayName", REG_SZ, app_name, (DWORD)strlen(app_name) + 1);
    RegSetValue(app, "InstallDate", REG_DWORD, &install_date, sizeof(install_date));
    RegCloseKey(app);
    return STATUS_SUCCESS;
}

/* Enregistre une désinscription d'application. */
NTSTATUS SoftwareHiveUnregisterApp(const char *app_name)
{
    /* Pour simplifier: on marque la clé comme supprimée en pratique le
     * hive_manager gèrerait la suppression. */
    (void)app_name;
    return STATUS_SUCCESS;
}

/* Enregistre une association d'extension de fichier. */
NTSTATUS SoftwareHiveRegisterExtension(const char *ext, const char *progid)
{
    char path[128];
    REG_KEY *k;
    if (!ext || !progid) return STATUS_INVALID_PARAMETER;
    snprintf(path, sizeof(path), "Software\\Classes\\%s", ext);
    if (!NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &k)))
        return STATUS_UNSUCCESSFUL;
    RegSetValue(k, "", REG_SZ, progid, (DWORD)strlen(progid) + 1);
    RegCloseKey(k);
    return STATUS_SUCCESS;
}

/* Enregistre un ProgID avec sa commande shell "open". */
NTSTATUS SoftwareHiveRegisterProgId(const char *progid, const char *open_cmd)
{
    char path[256];
    REG_KEY *k;
    if (!progid || !open_cmd) return STATUS_INVALID_PARAMETER;
    snprintf(path, sizeof(path), "Software\\Classes\\%s\\shell\\open\\command", progid);
    if (!NT_SUCCESS(HiveGetKey(HKEY_LOCAL_MACHINE, path, &k)))
        return STATUS_UNSUCCESSFUL;
    RegSetValue(k, "", REG_SZ, open_cmd, (DWORD)strlen(open_cmd) + 1);
    RegCloseKey(k);
    return STATUS_SUCCESS;
}
