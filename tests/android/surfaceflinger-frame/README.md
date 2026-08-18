# android/surfaceflinger-frame

Test de rendu d'une frame via SurfaceFlinger.

## Ce que fait l'app

- `FrameActivity` crée une `SurfaceView`.
- Sur `surfaceCreated`, `lockCanvas` → `drawCircle(50, 50, 20, RED)` →
  `unlockCanvasAndPost`.
- Affiche `"Hello, AfriOS!"` sur stdout après le post.

## Ce qu'on valide

- `surface_flinger.cpp` expose un `Surface` fonctionnel.
- `buffer_queue.cpp` alloue et poste un buffer graphique.
- `hwcomposer_hal.cpp` composite la frame (même sans GPU réel).
- `lockCanvas` / `unlockCanvasAndPost` ne deadlock pas.

## Validation visuelle (optionnelle)

Le manifest porte `expected_pixel = {x:50, y:50, color:"#FF0000"}`.
Le harness standard vérifie seulement `stdout_match` ; une extension
future pourra prendre un screenshot via `screencap` et vérifier le
pixel.

## Build (DEX)

```bash
javac FrameActivity.java
d8 --output frame.dex com/afrios/FrameActivity.class
```
