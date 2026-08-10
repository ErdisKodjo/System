# ios/coreanimation-bounce

Test d'animation CoreAnimation.

## Ce que fait l'app

1. `CALayer layer` → layer 100×100.
2. `CABasicAnimation animationWithKeyPath:@"position"` avec
   `fromValue`/`toValue`/`duration`.
3. `[layer addAnimation:bounce forKey:@"bounce"]`.
4. `CATransaction begin` / `setAnimationDuration:0.5` /
   `commit` après avoir changé `layer.position`.

## Ce qu'on valide

- `CoreAnimation/CALayer.m`, `CAAnimation.m`, `CATransaction.m` de
  `afros-incompat-engine` ne crashent pas sur un commit.
- Le runloop d'animation se déclenche (même sans render server réel,
  l'API ne doit pas deadlock).
- Le binding CoreGraphics (`CGColor.c`, `CGPath.c`) est cohérent.

## Build

```bash
clang -arch arm64-apple-darwin -framework QuartzCore \
      -framework Foundation -o coreanimation-bounce main.m
```
