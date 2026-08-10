---
rfc: 0001
title: Stabilize Loader API (Tier 1) for v1.0.0
status: Accepted
author: Agent API <api@afrios.dev>
created: 2026-08-11
target_version: 1.0.0
tier_impacted: Tier 1
---

# RFC 0001 — Stabilize Loader API (Tier 1) for v1.0.0

## Summary

Cette RFC documente **rétroactivement** la stabilisation de l'API
Loader de `afros-corebridge-core` en tant que **Tier 1 Stable** pour la
release v1.0.0. L'API Loader regroupe `AppDetect`,
`AppDetectBuffer`, `FormatAnalyze`, `ResolveDeps`, `DepListFree`,
`IntelligentLoad` et la table `loader_ops_t`. À partir de v1.0.0, toute
modification de signature ou de sémantique de ces fonctions nécessitera
un bump `MAJOR` et une nouvelle RFC acceptée.

## Motivation

L'API Loader est le point d'entrée **le plus utilisé** de CoreBridge :
toute décision de routage d'une application passe par `AppDetect` puis
`IntelligentLoad`. Sans contrat de stabilité formel :

- les développeurs tiers (CLI `afros-launch`, harness de compat-tests,
  intégrations desktop) ne peuvent pas construire sur CoreBridge sans
  risquer un cassage à chaque release ;
- les refactors internes (changements de cache, nouvelles heuristiques
  de score) peuvent involontairement modifier la signature publique et
  casser les consumers sans que personne ne le remarque avant la
  release ;
- il n'existe aucun moyen programmatique pour un consumer de vérifier
  qu'il link contre une version compatible (d'où l'ajout de
  `afros_corebridge_api_version()`).

Stabiliser l'API Loader est donc un pré-requis à la promesse de
stabilité de l'ensemble du module CoreBridge.

## Detailed Design

### Signature proposée (gelée)

```c
/* include/loader.h */

app_type_t      AppDetect(const char *path);
app_type_t      AppDetectBuffer(const void *buf, size_t len);
format_info_t   FormatAnalyze(const char *path, app_type_t type);
dep_list_t     *ResolveDeps(const char *path, app_type_t type);
void            DepListFree(dep_list_t *list);
runtime_handle_t IntelligentLoad(const char *path, const char *args);

const loader_ops_t *LoaderGetOps(void);
```

Aucune modification de signature par rapport au code existant
(`loader/app_detector.c`, `loader/format_analyzer.c`,
`loader/dependency_resolver.c`, `loader/intelligent_loader.c`). Cette
RFC **gel** l'existant, elle ne le modifie pas.

### Types gelés

Les types suivants, définis dans `include/loader.h`, sont également
Tier 1 Stable :

```c
typedef enum { /* app_type_t — ordre et valeurs gelés */
    APP_TYPE_UNKNOWN = 0,
    APP_TYPE_NATIVE,
    APP_TYPE_LINUX,
    APP_TYPE_WINDOWS,
    APP_TYPE_MACOS,
    APP_TYPE_ANDROID,
    APP_TYPE_HARMONY
} app_type_t;

typedef struct { /* format_info_t — champs et ordre gelés */
    app_type_t  type;
    uint32_t    arch;
    uint32_t    subsystem;
    uint32_t    bits;
    uint32_t    format_version;
    char        interpreter[MAX_INTERP_LEN];
    char        bundle_id[MAX_DEP_NAME_LEN];
    char        entry_name[MAX_DEP_NAME_LEN];
} format_info_t;

typedef struct { /* dep_entry_t */
    char name[MAX_DEP_NAME_LEN];
    char resolved_path[MAX_DEP_NAME_LEN];
    int  resolved;
} dep_entry_t;

typedef struct { /* dep_list_t — taille fixe, pas d'allocation interne */
    app_type_t  type;
    uint32_t    count;
    dep_entry_t entries[MAX_DEP_ENTRIES];
} dep_list_t;

typedef uint32_t runtime_handle_t;
#define INVALID_RUNTIME_HANDLE ((runtime_handle_t)0)

typedef struct { /* loader_ops_t — vtable */
    app_type_t        (*detect)(const char *path);
    format_info_t     (*analyze)(const char *path, app_type_t type);
    dep_list_t       *(*resolve_deps)(const char *path, app_type_t type);
    runtime_handle_t  (*load)(const char *path, const char *args);
} loader_ops_t;
```

### Contrats sémantiques gelés

| Fonction            | Precondition              | Postcondition                                                                     |
|---------------------|---------------------------|-----------------------------------------------------------------------------------|
| `AppDetect(path)`   | `path != NULL`, file existe | Retourne un `app_type_t` ∈ {UNKNOWN, NATIVE, LINUX, WINDOWS, MACOS, ANDROID, HARMONY}. |
| `AppDetectBuffer(buf, len)` | `buf != NULL`, `len >= 4` | Idem.                                                                             |
| `FormatAnalyze(path, type)` | `path != NULL`          | Retourne un `format_info_t` initialisé à zéro puis rempli. Si `type == APP_TYPE_UNKNOWN`, fait un `AppDetect(path)` implicite. |
| `ResolveDeps(path, type)` | `path != NULL`            | Retourne un `dep_list_t *` heap-allocué ou `NULL` sur échec. **Caller doit appeler `DepListFree`.** Maximum `MAX_DEP_ENTRIES` (64) entrées. |
| `DepListFree(list)` | `list` est un pointeur retourné par `ResolveDeps` (ou NULL). | Libère la mémoire. No-op si `list == NULL`.                                       |
| `IntelligentLoad(path, args)` | `path != NULL`          | Retourne un `runtime_handle_t > 0` ou `INVALID_RUNTIME_HANDLE`. Le handle est caché : deux appels avec le même `path` retournent le même handle. `args` est actuellement ignoré par le decision engine (réservé pour usage futur — la signature est gelée, pas le comportement interne). |
| `LoaderGetOps()`    | aucune.                   | Retourne un pointeur vers une `loader_ops_t` statique. Ne pas libérer.            |

### Exemple d'utilisation

```c
#include "afros_corebridge.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <binary>\n", argv[0]); return 1; }

    app_type_t t = AppDetect(argv[1]);
    printf("type = %d\n", (int)t);

    format_info_t fi = FormatAnalyze(argv[1], t);
    printf("arch=%u bits=%u subsystem=%u interp=%s\n",
           fi.arch, fi.bits, fi.subsystem, fi.interpreter);

    dep_list_t *deps = ResolveDeps(argv[1], t);
    if (deps) {
        for (uint32_t i = 0; i < deps->count; i++)
            printf("  dep[%u] = %s (%s)\n", i,
                   deps->entries[i].name,
                   deps->entries[i].resolved ? deps->entries[i].resolved_path
                                             : "NOT FOUND");
        DepListFree(deps);
    }

    runtime_handle_t h = IntelligentLoad(argv[1], NULL);
    printf("handle = %u\n", (unsigned)h);
    return 0;
}
```

### Impact sur les sources

Aucun changement de code — cette RFC est purement **documentaire et
contractuelle**. Les fichiers impactés sont :

- `include/loader.h` : ajout d'un commentaire `/* Tier 1 Stable — see
  API.md §5 */` en tête de chaque fonction (déjà présent dans la
  version gelée).
- `API.md` §5 : spécification formelle.
- `CHANGELOG.md` : entrée sous `[1.0.0]` → `Added`.

## Alternatives

### Alternative A : Ne pas geler l'API Loader, la marquer Tier 2 Beta

- Comment : traiter l'API Loader comme le Version Management API —
  stable à l'intérieur d'une `MINOR` mais susceptible d'évoluer entre
  deux `MINOR`.
- Pourquoi rejetée : l'API Loader est appelée par **chaque** lancement
  d'application. La moindre incertitude sur sa stabilité empêche
  toute adoption tierce. Le coût d'un gel Tier 1 est faible (l'API
  existante fonctionne et répond aux cas d'usage courants) ; le coût
  d'une instabilité perçue est élevé (pas de third-party developers).

### Alternative B : Geler seulement `AppDetect` et `IntelligentLoad`, laisser le reste en Tier 2

- Comment : minimiser la surface gelée en n'incluant que les deux
  fonctions les plus utilisées.
- Pourquoi rejetée : `FormatAnalyze` est nécessaire pour qu'un caller
  puisse afficher des infos sur l'app (arch, subsystem) avant de la
  lancer ; `ResolveDeps` est nécessaire pour les outils de packaging.
  Les geler ensemble évite des contradictions (un caller qui voit un
  Tier 1 `AppDetect` mais un Tier 2 `FormatAnalyze` ne sait pas quelle
  garantie appliquer).

### Alternative C : Réécrire l'API Loader en C++ avec une classe `Loader`

- Comment : exposer `class Loader { static app_type_t detect(...); ... }`.
- Pourquoi rejetée : CoreBridge doit rester linkable depuis du C pur
  (le kernel AfriOS est en C, le HAL est en C). Une API C++ casserait
  cette propriété.

## Risks

### Rétro-compatibilité

- Aucun risque : l'API existante est gelée telle quelle. Aucun
  consumer n'est impacté.
- Les futurs changements devront passer par une RFC + bump `MAJOR`.

### Performance

- Aucun impact : aucun changement de code.
- Le contrat ne fige pas les performances (un futur patch pourra
  accélérer `AppDetect` par exemple), seulement les signatures et la
  sémantique observable.

### Sécurité

- `AppDetect` et `AppDetectBuffer` lisent des magic bytes : un fichier
  malicieux pourrait tenter de déclencher un buffer over-read. Le
  contrat garantit que les fonctions ne lisent jamais plus de `len`
  bytes (pour `AppDetectBuffer`) ou plus que la taille du fichier
  (pour `AppDetect`). Cette garantie fait partie du contrat Tier 1 et
  devra être testée par des tests fuzzing dans `tests/`.

### Complexité

- Aucune complexité ajoutée : pas de nouveau code.

## Rollout Plan

### Version cible

`v1.0.0` — release du 2026-08-11. Cette RFC est **rétroactive** : elle
documente une décision déjà appliquée dans le code.

### Déprécations

Aucune. L'API Loader n'avait pas de précurseur déprécié.

### Migration des consommateurs

Aucune action requise : les consumers existants (CLI runtime-selector,
compat-test-harness) utilisent déjà ces signatures.

### Documentation

- `API.md` §5 : spécification formelle (déjà ajoutée).
- `CHANGELOG.md` `[1.0.0]` → `Added` : mention explicite (déjà ajouté).
- `include/loader.h` : commentaires Doxygen mis à jour pour pointer
  vers `API.md` §5.

### Tracking

- Tracking issue : `#0001` (à créer dans GitHub).
- PR d'implémentation : la PR qui merge cette RFC + `API.md` +
  `CHANGELOG.md` + les commentaires `loader.h`.
- Tests : ajouter `tests/unit/test_loader_api_freeze.c` qui vérifie
  que les types et signatures correspondent au contrat (test
  compile-time via `_Static_assert`).

## Open Questions

- Q1 : Faut-il inclure `LoaderGetOps()` dans le gel Tier 1, ou la
  laisser Tier 2 puisqu'elle n'est utilisée que par les plugins ?
  **RESOLVED** : Oui, l'inclure. Elle est appelée par le
  `selection_engine` et des plugins tiers pourraient en dépendre.
- Q2 : La taille `MAX_DEP_ENTRIES = 64` est-elle suffisante ? Certaines
  applications Windows complexes ont > 64 DLLs.
  **RESOLVED** : Garder 64 pour v1.0.0. Si un caller a besoin de
  plus, il peut appeler `ResolveDeps` plusieurs fois avec des
  prefixes différents. Une future RFC (post-1.0) pourrait introduire
  une variante `ResolveDepsEx(path, type, max_entries)`.

## References

- Code source : `loader/app_detector.c`, `loader/format_analyzer.c`,
  `loader/dependency_resolver.c`, `loader/intelligent_loader.c`.
- Header : `include/loader.h`.
- Spec API : `API.md` §5.
- Changelog : `CHANGELOG.md` `[1.0.0]`.
- Issue de tracking : à créer.

---

*Première RFC du dépôt `afros-corebridge-core`. Sert d'exemple de
format et de niveau de détail attendu pour les RFCs futures.*
