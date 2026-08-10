# linux/hello-elf

Test Linux natif minimal — **baseline** de la suite.

## Ce que fait l'app

- `printf("Hello, AfriOS!\n")` puis `return 0`.
- Compilée en ELF natif (x86_64 ou arm64 selon l'hôte).

## Ce qu'on valide

- Le runtime Linux de `afros-corebridge-core` exécute un binaire
  natif sans couche de compatibilité.
- `printf` via la libc hôte fonctionne.
- Si ce test échoue, **tous les autres tests sont suspects** — c'est
  le canari de la suite.

## Build

```bash
make  # gcc ou clang
```
