/*
 * hello-elf/source.c — Test Linux #1 : Hello World ELF natif.
 *
 * Programme C minimal compilé en ELF natif. Sert de baseline : si ce
 * test échoue, le problème est dans le runtime Linux lui-même, pas
 * dans une couche de compatibilité.
 */
#include <stdio.h>

int main(void) {
    printf("Hello, AfriOS!\n");
    return 0;
}
