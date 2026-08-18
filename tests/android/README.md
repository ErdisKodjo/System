# Tests de compatibilité Android

Valident `afros-androsandbox` (ART, binder, surfaceflinger, activity
manager, package manager).

## Liste des tests

| Test                    | Ce que ça valide                                   |
|-------------------------|----------------------------------------------------|
| `hello-apk`             | DEX loader + runtime ART + System.out              |
| `binder-roundtrip`      | IPC Binder service ↔ client                        |
| `activity-lifecycle`    | Cycle de vie Activity (6 callbacks, ordre)         |
| `surfaceflinger-frame`  | SurfaceView + Canvas + SurfaceFlinger              |

## Build

Tous les tests nécessitent `javac` (JDK 8+) et `d8` (Android SDK
build-tools). Si absents, les `.dex` sont supposés précompilés.

```bash
for d in */; do
    (cd "$d" && javac *.java && d8 --output *.dex com/afrios/*.class)
done
```

## Exécution

```bash
python3 ../compat-test-harness.py --platform android
```

Le harness invoque `afros-launch --runtime=android --entry=<class> <dex>`,
qui route vers `dalvikvm` dans `afros-androsandbox`.
