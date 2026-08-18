/*
 * hello-world/source.c — Test Windows #1 : exécutable minimal Win32.
 *
 * Invoque printf() via la libc Microsoft, puis ExitProcess(0) pour
 * terminer proprement. Valide que le PE loader + la libc de afros-winbridge
 * fonctionnent.
 */
#include <stdio.h>
#include "win32-stubs.h"
#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
    printf("Hello, AfriOS!\n");
    fflush(stdout);
    ExitProcess(0);
    return 0; /* unreachable */
}
