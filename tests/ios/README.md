# Tests de compatibilité iOS / macOS

Valident `afros-incompat-engine` (Mach-O loader, runtime ObjC, frameworks
Foundation/UIKit/CoreAnimation via la couche Darling).

## Liste des tests

| Test                    | Ce que ça valide                                  |
|-------------------------|---------------------------------------------------|
| `hello-app`             | Mach-O loader + NSLog + printf                    |
| `foundation-nsstring`   | NSString : stringWithFormat, length, charAtIndex  |
| `uikit-window`          | UIWindow + makeKeyAndVisible                      |
| `coreanimation-bounce`  | CALayer + CABasicAnimation + CATransaction        |

## Build

Tous les tests supposent `clang` cross-Darwin
(`clang -arch arm64-apple-darwin`) disponible, ou un binaire Mach-O
précompilé à côté. Sans toolchain, le build est skippé et le test
marqué `SKIP`.

```bash
for d in */; do
    (cd "$d" && clang -arch arm64-apple-darwin \
        -framework Foundation -o "$(basename $d)" main.m)
done
```

## Exécution

```bash
python3 ../compat-test-harness.py --platform ios
```

Le harness invoque `afros-launch --runtime=ios <binary>`, qui route
vers la couche Darling de `afros-incompat-engine`.
