# harmonyos/hello-hap

Test HarmonyOS minimal — HAP Hello World.

## Ce que fait l'app

- `HelloAbility.js` exporte un objet avec `onStart` qui log
  `"Hello, AfriOS!"`.
- `module.json` décrit le bundle `com.afrios.hello`.
- Le HAP est un ZIP de `module.json` + `abilities/HelloAbility.js`.

## Ce qu'on valide

- Le HAP loader de `afros-harmonygate` (ZIP + module.json parser).
- Le runtime Ability (`ability_runtime/ability_lifecycle.cpp`) déclenche
  `onStart`.
- Le runtime JS exécute `console.info` redirigé vers stdout.

## Build (HAP)

```bash
mkdir -p abilities && cp HelloAbility.js abilities/
zip -r hello.hap module.json abilities/
```

Si `zip` est absent, le harness peut le générer à la volée via Python.
