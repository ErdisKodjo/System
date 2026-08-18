# windows/hello-world

Test minimal de compatibilité Windows.

## Ce que fait l'app

- `printf("Hello, AfriOS!\n")` puis `ExitProcess(0)`.
- Compilée en PE x86_64 avec `x86_64-w64-mingw32-gcc`.

## Ce qu'on valide

- Le PE loader de `afros-winbridge` charge un exécutable simple.
- La libc Microsoft (msvcrt) fonctionne (`printf`, `fflush`).
- `ExitProcess` termine proprement avec code 0.

## Build

```bash
make  # nécessite mingw-w64
```
