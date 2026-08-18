# windows/registry-access

Test d'accès au registry Windows.

## Ce que fait l'app

1. `RegCreateKeyExA(HKCU, "Software\\AfriOS\\Test", ...)`.
2. `RegSetValueExA("Greeting", "Hello, AfriOS!")`.
3. `RegCloseKey`.
4. `RegOpenKeyExA` en lecture.
5. `RegQueryValueExA("Greeting", buf)`.
6. Affiche la valeur lue.

## Ce qu'on valide

- L'émulateur de hive `software_hive.c` de `afros-winbridge`.
- La persistance écriture-then-lecture sur la même clé.
- Le cache registry ne renvoie pas de données périmées.

## Build

```bash
make  # nécessite mingw-w64
```
