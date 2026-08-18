# Tests de compatibilité HarmonyOS

Valident `afros-harmonygate` (HAP loader, Ability runtime, SoftBus,
distributed data sync).

## Liste des tests

| Test                  | Ce que ça valide                                   |
|-----------------------|----------------------------------------------------|
| `hello-hap`           | HAP loader + Ability runtime + console.info        |
| `ability-lifecycle`   | Cycle de vie Ability (6 callbacks, ordre)          |
| `softbus-discovery`   | SoftBus discovery entre deux devices simulés       |

## Build (HAP)

Chaque test produit un fichier `.hap` (ZIP de `module.json` +
`abilities/*.js`).

```bash
for d in */; do
    (cd "$d" && mkdir -p abilities && cp *.js abilities/ && \
        zip -r "$(basename $d)".hap module.json abilities/)
done
```

Si `zip` est absent, le harness peut générer le HAP via Python
(`zipfile` stdlib).

## Exécution

```bash
python3 ../compat-test-harness.py --platform harmonyos
```

Le harness invoque `afros-launch --runtime=harmony <hap>`, qui route
vers `afros-harmonygate`.
