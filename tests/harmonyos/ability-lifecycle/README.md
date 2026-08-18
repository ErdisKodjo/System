# harmonyos/ability-lifecycle

Test du cycle de vie d'une Ability HarmonyOS.

## Ce que fait l'app

- `LifecycleAbility.js` logge chaque callback dans un array `LOG` et
  sur `console.info` :
  - `onStart` → `onActive` → `onInactive` → `onBackground`
  - → `onForeground` → `onStop`.
- Sur `onStop`, affiche `"Hello, AfriOS!"`.

## Ce qu'on valide

- `ability_lifecycle.cpp` déclenche les 6 callbacks dans l'ordre.
- `ability_context.cpp` gère les transitions (active ↔ inactive,
  foreground ↔ background).
- `ability_manager.cpp` orchestre le cycle complet sans deadlock.

## Build (HAP)

```bash
mkdir -p abilities && cp LifecycleAbility.js abilities/
zip -r lifecycle.hap module.json abilities/
```
