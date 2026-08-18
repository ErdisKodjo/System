# android/activity-lifecycle

Test du cycle de vie d'une Activity.

## Ce que fait l'app

- `LifecycleActivity` surcharge les 6 callbacks de cycle de vie et
  loggue chacun via `Log.i` + `System.out.println`.

## Ce qu'on valide

- L'`activity_manager_service.cpp` pilote correctement les transitions
  d'état (created → started → resumed → paused → stopped → destroyed).
- L'`activity_stack.cpp` gère l'ordre des callbacks.
- `Log.i` est redirigé vers stdout (pas de logcat réel dans le sandbox).

## Build (DEX)

```bash
javac LifecycleActivity.java
d8 --output lifecycle.dex com/afrios/LifecycleActivity.class
```

Le harness valide que les 6 lignes apparaissent **dans l'ordre
attendu** dans stdout. Le champ `expected_log_order` du manifest
documente cet ordre (le harness standard vérifie seulement
`expected_output` — l'ordre strict est une extension future).
