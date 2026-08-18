/*
 * file-io/source.c — Test Windows #2 : I/O fichier Win32.
 *
 * Crée test.txt, écrit "Hello, AfriOS!" via WriteFile, ferme,
 * rouvre en lecture, lit via ReadFile, affiche le contenu.
 * Valide CreateFile/WriteFile/ReadFile/CloseHandle dans afros-winbridge.
 */
#include <stdio.h>
#include "win32-stubs.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define TEST_FILE "test.txt"
#define TEST_DATA "Hello, AfriOS!"
#define BUF_LEN   64

int main(void) {
    HANDLE h;
    DWORD written;
    DWORD read;
    char buf[BUF_LEN] = {0};

    /* --- Écriture --- */
    h = CreateFileA(TEST_FILE, GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("FAIL: CreateFile(write) err=%lu\n", GetLastError());
        ExitProcess(1);
    }
    if (!WriteFile(h, TEST_DATA, (DWORD)sizeof(TEST_DATA) - 1,
                   &written, NULL) || written != sizeof(TEST_DATA) - 1) {
        printf("FAIL: WriteFile err=%lu\n", GetLastError());
        CloseHandle(h);
        ExitProcess(1);
    }
    CloseHandle(h);

    /* --- Lecture --- */
    h = CreateFileA(TEST_FILE, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("FAIL: CreateFile(read) err=%lu\n", GetLastError());
        ExitProcess(1);
    }
    if (!ReadFile(h, buf, BUF_LEN - 1, &read, NULL)) {
        printf("FAIL: ReadFile err=%lu\n", GetLastError());
        CloseHandle(h);
        ExitProcess(1);
    }
    buf[read] = '\0';
    CloseHandle(h);

    printf("%s\n", buf);
    ExitProcess(0);
    return 0;
}
