/*
 * registry-access/source.c — Test Windows #4 : accès registry.
 *
 * Crée HKEY_CURRENT_USER\Software\AfriOS\Test, y écrit "Hello, AfriOS!"
 * dans la valeur "Greeting", ferme, rouvre, relit, et affiche.
 * Valide l'émulateur de hive de afros-winbridge.
 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    HKEY hKey;
    DWORD disp;
    LONG rc;

    /* --- Création + écriture --- */
    rc = RegCreateKeyExA(HKEY_CURRENT_USER,
                         "Software\\AfriOS\\Test",
                         0, NULL, 0,
                         KEY_WRITE, NULL, &hKey, &disp);
    if (rc != ERROR_SUCCESS) {
        printf("FAIL: RegCreateKeyExA err=%ld\n", rc);
        ExitProcess(1);
    }

    const char *data = "Hello, AfriOS!";
    rc = RegSetValueExA(hKey, "Greeting", 0, REG_SZ,
                        (const BYTE *)data,
                        (DWORD)(strlen(data) + 1));
    if (rc != ERROR_SUCCESS) {
        printf("FAIL: RegSetValueExA err=%ld\n", rc);
        RegCloseKey(hKey);
        ExitProcess(1);
    }
    RegCloseKey(hKey);

    /* --- Lecture --- */
    rc = RegOpenKeyExA(HKEY_CURRENT_USER,
                       "Software\\AfriOS\\Test",
                       0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS) {
        printf("FAIL: RegOpenKeyExA err=%ld\n", rc);
        ExitProcess(1);
    }

    char buf[64] = {0};
    DWORD bufLen = sizeof(buf);
    DWORD type;
    rc = RegQueryValueExA(hKey, "Greeting", NULL, &type,
                          (LPBYTE)buf, &bufLen);
    if (rc != ERROR_SUCCESS) {
        printf("FAIL: RegQueryValueExA err=%ld\n", rc);
        RegCloseKey(hKey);
        ExitProcess(1);
    }
    RegCloseKey(hKey);

    printf("%s\n", buf);
    ExitProcess(0);
    return 0;
}
