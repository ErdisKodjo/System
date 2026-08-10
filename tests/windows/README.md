# Tests de compatibilité Windows

Valident `afros-winbridge` (PE loader, registry, COM, syscall translator)
et `afros-dxvk` (GDI via DXVK).

## Liste des tests

| Test              | Ce que ça valide                                  |
|-------------------|---------------------------------------------------|
| `hello-world`     | PE loader + libc (printf, ExitProcess)            |
| `file-io`         | CreateFile/WriteFile/ReadFile/CloseHandle         |
| `gdi-draw`        | GDI : GetDC, Rectangle, CreateSolidBrush          |
| `registry-access` | RegCreateKey/RegSetValueEx/RegQueryValueEx        |
| `com-basic`       | CoInitialize + CoCreateInstance(DOMDocument)      |

## Build

Tous les tests utilisent `x86_64-w64-mingw32-gcc` (mingw-w64). Si absent,
le build échoue silencieusement et le harness marque le test `SKIP`.

```bash
for d in */; do make -C "$d"; done
```

## Exécution

```bash
python3 ../compat-test-harness.py --platform windows
```
