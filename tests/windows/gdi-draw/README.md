# windows/gdi-draw

Test de dessin GDI minimal.

## Ce que fait l'app

1. `GetDC(NULL)` → HDC pour tout l'écran.
2. `CreateSolidBrush(RGB(255,0,0))` → brush rouge.
3. `Rectangle(hdc, 10, 10, 100, 100)` → dessine un rectangle.
4. `DeleteObject`, `ReleaseDC`.

## Ce qu'on valide

- `afros-dxvk` expose un device context GDI fonctionnel.
- Le pipeline Wine GDI → DXVK → Vulkan ne crash pas.
- `CreateSolidBrush`, `Rectangle`, `SelectObject` fonctionnent.

## Build

```bash
make  # nécessite mingw-w64
```
