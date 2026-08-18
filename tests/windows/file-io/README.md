# windows/file-io

Test d'I/O fichier via l'API Win32.

## Ce que fait l'app

1. `CreateFileA("test.txt", GENERIC_WRITE, ...)` avec `CREATE_ALWAYS`.
2. `WriteFile` de la chaîne `"Hello, AfriOS!"`.
3. `CloseHandle`.
4. `CreateFileA` en lecture, `ReadFile`, `CloseHandle`.
5. Affiche le contenu lu.

## Ce qu'on valide

- Le path translator de `afros-winbridge` (chemins Windows → Linux).
- L'émulation NTFS / attributs de fichiers.
- Cohérence écriture-then-lecture sur le même fichier.

## Build

```bash
make  # nécessite mingw-w64
```
