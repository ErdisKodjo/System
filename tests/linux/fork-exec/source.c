/*
 * fork-exec/source.c — Test Linux #2 : fork + execve + waitpid.
 *
 * Le parent fork(), l'enfant execve("/bin/echo", ["echo","child"], NULL),
 * le parent waitpid(). Valide que afros-core gère ces syscalls.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Enfant */
        char *argv[] = {"echo", "child", NULL};
        char *envp[] = {NULL};
        execve("/bin/echo", argv, envp);
        perror("execve");
        _exit(127);
    }

    /* Parent */
    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    if (w < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("Hello, AfriOS!\n");
        return 0;
    }
    printf("FAIL: child exit status=%d\n", status);
    return 1;
}
