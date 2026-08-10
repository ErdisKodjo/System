# windows/com-basic

Test d'instanciation COM.

## Ce que fait l'app

1. `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)`.
2. `CoCreateInstance(CLSID_DOMDocument, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument)`.
3. Si OK : `printf("Hello, AfriOS!")`.
4. `Release`, `CoUninitialize`.

## Ce qu'on valide

- Le runtime COM `com_runtime.c` de `afros-winbridge`.
- Le registry des CLSID (le `com_basic` exige que `CLSID_DOMDocument`
  soit enregistré — soit via le hive `software`, soit via un fallback
  dans le runtime).
- Le marshaler / proxy_stub ne crash pas sur un simple QueryInterface +
  Release.

## Build

```bash
make  # nécessite mingw-w64 avec g++
```
