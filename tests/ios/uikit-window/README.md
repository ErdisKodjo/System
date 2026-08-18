# ios/uikit-window

Test de création d'un UIWindow.

## Ce que fait l'app

1. `HelloAppDelegate` implémente `UIApplicationDelegate`.
2. Dans `application:didFinishLaunchingWithOptions:` :
   - `[[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]`
   - `makeKeyAndVisible`
3. Affiche `"Hello, AfriOS!"`.

## Ce qu'on valide

- `UIKit/UIWindow.m` et `UIKit/UIApplication.m` de
  `afros-incompat-engine` ne crashent pas sur `makeKeyAndVisible`.
- `UIScreen.mainScreen` expose un `bounds` valide.
- `UIApplicationMain` dispatche le delegate correctement.

## Build

```bash
clang -arch arm64-apple-darwin -framework UIKit -framework Foundation \
      -o uikit-window main.m
```
