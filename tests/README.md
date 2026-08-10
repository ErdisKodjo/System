# AfriOS Compatibility Test Suite

Cette suite établit une **baseline mesurable** de la compatibilité réelle
d'AfriOS avec les applications des 5 plateformes supportées :
**Windows, Android, iOS/macOS, HarmonyOS, Linux natif**.

Les couches de compatibilité (`afros-winbridge`, `afros-androsandbox`,
`afros-incompat-engine`, `afros-harmonygate`, et le runtime Linux natif)
ont été implémentées mais jamais testées contre des applications réelles.
Cette suite produit un score 0-100 par test, agrégé par plateforme, pour
objectiver le niveau de compatibilité atteint à chaque commit.

## Objectif

- Mesurer la compatibilité réelle (pas juste la compilation du code) des
  couches AfriOS contre des applications représentatives de chaque
  plateforme.
- Fournir un signal de régression : un commit qui casse un test existant
  est immédiatement visible.
- Documenter les fonctionnalités supportées via des tests lisibles
  (chaque test est un mini-programme commenté).

## Pré-requis

1. **Toolchain hôte** :
   - `python3 >= 3.10` (pour le harness)
   - `bash >= 4` (pour l'orchestrateur)
   - `gcc` ou `clang` (tests Linux natifs)
2. **Toolchain AfriOS** :
   - `afros-launch` sur le `PATH` (point d'entrée utilisateur de
     l'orchestrateur `afros-corebridge-core`). Si absent, les tests
     sont marqués `SKIP` avec la raison `afros-launch not installed`
     plutôt que de crasher — utile pour valider la découverte sans
     runtime complet.
3. **Dépendances externes** : `external/` doit être peuplé via :

   ```bash
   ./scripts/fetch-deps.sh all
   ```

   Cf. `external/README.md` pour le détail. Sans cela, certaines
   toolchains cross-compil (mingw-w64 pour Windows, clang cross pour
   Darwin, d8 pour Android DEX) peuvent manquer ; les tests
   concernés seront marqués `SKIP` (binaire manquant).

## Lancer la suite

### Tout lancer

```bash
./tests/run-compat-tests.sh
```

Génère `tests/results/report-<timestamp>.md` et
`tests/results/results-<timestamp>.json`.

### Une plateforme

```bash
python3 tests/compat-test-harness.py --platform windows
# ou
./tests/run-compat-tests.sh --platform linux
```

### Un test précis

```bash
python3 tests/compat-test-harness.py --test windows/hello-world
```

### Découverte sans exécution (dry-run)

```bash
python3 tests/compat-test-harness.py --dry-run
```

Liste tous les tests découverts sans rien exécuter — utile en CI pour
vérifier que les manifests sont bien formés.

### Logs détaillés

```bash
./tests/run-compat-tests.sh --verbose
```

### Parallélisme

```bash
python3 tests/compat-test-harness.py --jobs 4
```

## Interpréter les résultats

Chaque test produit un **score 0-100** calculé comme somme pondérée de
critères définis dans son `test.json`. Les critères standards sont :

| Critère                   | Poids défaut | Signification                              |
|---------------------------|--------------|--------------------------------------------|
| `stdout_match`            | 50           | La chaîne `expected_output` apparaît dans stdout. |
| `exit_code_zero`          | 30           | Le process s'est terminé avec exit code 0. |
| `completes_under_timeout` | 20           | Pas de timeout, durée ≤ `timeout_ms`.      |

Un test est `PASS` si `score == 100`, sinon `FAIL`. Un test `SKIP`
n'est ni réussi ni échoué — il est exclu du dénominateur.

Le rapport final agrège :

- **Par plateforme** : nombre de tests, score moyen, nombre de réussis.
- **Par test** : score, durée, exit code, détail des critères
  (`criterion=✓` / `criterion=✗`).
- **Sorties brutes** : stdout/stderr tronqués à 4 KB pour le debug.

### Échelle de compatibilité

| Score moyen plateforme | Interprétation                          |
|------------------------|-----------------------------------------|
| 90-100                 | Compatibilité quasi-native.             |
| 70-89                  | Compatibilité utilisable, cas-limites.  |
| 50-69                  | Démo jouable, apps réelles partielles.  |
| 30-49                  | Chargement OK, exécution partielle.     |
| 0-29                   | Couche présente mais non fonctionnelle. |

Cette baseline est attendue basse au démarrage — l'objectif est de la
faire monter avec chaque correctif.

## Architecture du harness

```
tests/
├── run-compat-tests.sh     Orchestrateur bash (build + invoke)
├── compat-test-harness.py  Framework Python
├── <platform>/
│   └── <test-name>/
│       ├── test.json       Manifest (nom, binaire, scoring, …)
│       ├── source.c/.cpp/.m/.java/.js   Source de l'app
│       ├── Makefile        (si compilation nécessaire)
│       └── README.md       Description courte
└── results/                Rapports générés (gitignored)
```

### Flux d'exécution

1. `run-compat-tests.sh` itère sur les 5 plateformes.
2. Pour chaque plateforme, il invoque `make` dans chaque sous-dossier
   qui a un `Makefile` (build silencieux ; les échecs de build ne sont
   pas fatals — le harness fera un `SKIP` du test).
3. Il appelle ensuite `compat-test-harness.py --platform <name>`.
4. Le harness :
   - **découvre** les tests en scannant `tests/<platform>/*/test.json` ;
   - construit un adaptateur par plateforme
     (`GenericAfriOSTest`, `AndroidTest`, `IOSTest`, `HarmonyOSTest`,
     `LinuxTest`) qui sait comment invoquer `afros-launch` ;
   - exécute chaque test via `subprocess.run` avec timeout ;
   - capture stdout/stderr/exit_code/duration_ms ;
   - calcule le score selon les poids du manifest ;
   - émet `results/results-<ts>.json` + `results/report-<ts>.md`.

### Adaptateurs par plateforme

| Plateforme | Adaptateur       | Ligne de commande générée                          |
|------------|------------------|----------------------------------------------------|
| windows    | GenericAfriOSTest| `afros-launch <binary>`                            |
| android    | AndroidTest      | `afros-launch --runtime=android [--entry=...] <b>` |
| ios        | IOSTest          | `afros-launch --runtime=ios <binary>`              |
| harmonyos  | HarmonyOSTest   | `afros-launch --runtime=harmony <binary>`          |
| linux      | LinuxTest        | `afros-launch --runtime=linux <binary>`            |

## Ajouter un nouveau test

1. Créer un sous-dossier `tests/<platform>/<test-name>/`.
2. Y placer :
   - `test.json` (manifest — cf. schéma ci-dessous) ;
   - le source de l'app (`source.c`, `main.m`, `MainActivity.java`, …) ;
   - un `Makefile` si compilation nécessaire ;
   - un `README.md` d'une dizaine de lignes.
3. Le harness le découvrira automatiquement au prochain run.
4. Vérifier avec `python3 tests/compat-test-harness.py --dry-run`.

### Schéma du manifest `test.json`

```json
{
    "name": "hello-world",
    "description": "Hello World Windows executable",
    "binary": "hello.exe",
    "expected_output": "Hello, AfriOS!",
    "timeout_ms": 5000,
    "scoring": {
        "stdout_match": 50,
        "exit_code_zero": 30,
        "completes_under_timeout": 20
    }
}
```

- `name` : identifiant court, unique dans la plateforme.
- `binary` : chemin relatif au dossier du test.
- `expected_output` : sous-chaîne recherchée dans stdout. Vide = pas
  de critère `stdout_match` (toujours vrai).
- `timeout_ms` : timeout d'exécution (défaut 5000).
- `scoring` : poids des critères. La somme peut dépasser 100 — le
  score est plafonné à 100. Si absent, les poids par défaut sont
  appliqués (50/30/20).

### Bonnes pratiques

- Un test doit pouvoir s'exécuter **indépendamment** (pas de dépendance
  à un autre test).
- Le source doit être **compilable** avec un toolchain standard (gcc,
  mingw-w64, clang, javac+d8) — ne pas hardcoder des chemins absolus.
- Le `README.md` du test décrit : ce que fait l'app, ce qu'on valide,
  dépendances éventuelles.
- Les tests longs (> 5 s) doivent expliciter un `timeout_ms` adapté.

## Vérification rapide

```bash
# (1) Syntaxe bash
bash -n tests/run-compat-tests.sh

# (2) Découverte sans exécution
python3 tests/compat-test-harness.py --dry-run

# (3) Validité JSON de tous les manifests
for f in tests/*/*/test.json; do
    python3 -c "import json,sys; json.load(open('$f'))" || echo "BAD: $f"
done

# (4) Compilation des tests Linux (baseline)
make -C tests/linux/hello-elf
make -C tests/linux/fork-exec
```

## Limitations connues

- Les tests cross-compilés (Windows PE via mingw, Mach-O via clang
  cross, DEX via d8) nécessitent les toolchains correspondants. Sans
  eux, le build est skippé et le test marqué `SKIP`.
- `afros-launch` doit exister sur le `PATH` ; sans lui, tous les tests
  sont skippés (mais le `--dry-run` fonctionne).
- Les tests Android `binder-roundtrip` et `surfaceflinger-frame`
  nécessitent un environnement Android simulé complet ; en l'absence
  d'`afros-androsandbox` fonctionnel, ils échoueront proprement.
- Le test `softbus-discovery` nécessite deux instances simulées ; le
  harness les lance séquentiellement et vérifie qu'elles se trouvent.

## Voir aussi

- `../AfriOS/AfriOS/OS/afros-corebridge-core/docs/compatibility-guide.md`
- `../AfriOS/AfriOS/OS/afros-corebridge-core/docs/runtime-api.md`
- `../external/README.md` — peupler les dépendances externes
- `../scripts/fetch-deps.sh` — cloner les sources upstream
