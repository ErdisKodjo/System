# linux/fork-exec

Test Linux natif — fork + execve + waitpid.

## Ce que fait l'app

1. Parent `fork()`.
2. Enfant : `execve("/bin/echo", ["echo","child"], NULL)`.
3. Parent : `waitpid()`.
4. Si enfant termine avec code 0, affiche `"Hello, AfriOS!"`.

## Ce qu'on valide

- `fork()` duplique correctement le processus (afros-core scheduler).
- `execve()` charge un nouveau binaire ELF depuis le VFS.
- `waitpid()` bloque puis récupère le statut de l'enfant.
- Le path `/bin/echo` est résolu par le VFS.

## Build

```bash
make  # gcc ou clang
```
