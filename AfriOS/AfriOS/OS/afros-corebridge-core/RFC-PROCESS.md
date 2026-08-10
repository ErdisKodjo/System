# Processus RFC — afros-corebridge-core

**Objet :** gouvernance des évolutions de l'API publique de
`afros-corebridge-core`.
**Portée :** tout changement touchant l'une des APIs listées dans
[`API.md`](API.md) (Tier 1, Tier 2 ou Tier 3).
**Version du processus :** 1.0 — gelée avec l'API v1.0.0.

---

## 1. Quand une RFC est-elle requise ?

Une RFC (Request For Comments) est obligatoire pour :

| Changement                                                | RFC requise ? | Bump      |
|-----------------------------------------------------------|---------------|-----------|
| Modification de signature d'une fonction Tier 1           | **Oui**       | `MAJOR`   |
| Retrait d'une fonction Tier 1                             | **Oui**       | `MAJOR`   |
| Ajout / retrait d'un énumérateur Tier 1                   | **Oui**       | `MAJOR`   |
| Modification de sémantique d'une fonction Tier 1          | **Oui**       | `MAJOR`   |
| Ajout d'une nouvelle fonction Tier 1                      | **Oui**       | `MINOR`   |
| Ajout / retrait d'une fonction Tier 2                     | **Oui**       | `MINOR`   |
| Ajout / retrait d'une fonction Tier 3                     | **Oui**       | `PATCH`   |
| Ajout d'un champ en fin de struct Tier 2                  | **Non** (note dans CHANGELOG) | `MINOR` |
| Bug fix sans changement de signature                      | **Non**       | `PATCH`   |
| Refactor interne non visible publiquement                 | **Non**       | `PATCH`   |
| Doc, commentaires, exemples                               | **Non**       | `PATCH`   |

**Règle d'or** : en cas de doute, ouvrez une RFC. Le coût d'une RFC
supplémentaire est négligeable comparé au coût d'un breaking change
non documenté.

---

## 2. Format d'une RFC

Chaque RFC est un fichier Markdown dans `rfcs/NNNN-<slug>.md` où
`NNNN` est le numéro de PR (commençant à 0001) et `<slug>` est un
identifiant kebab-case court (ex : `stabilize-loader-api`).

Le template officiel est [`rfcs/0000-template.md`](rfcs/0000-template.md).
Il contient les sections obligatoires suivantes :

1. **Title** — titre court.
2. **Summary** — 2-3 phrases.
3. **Motivation** — pourquoi ce changement ? Quel problème résout-il ?
4. **Detailed Design** — signatures proposées, exemples de code,
  changements de headers, impact sur les structs.
5. **Alternatives** — au moins une alternative considérée et rejetée.
6. **Risks** — rétro-compatibilité, performance, sécurité, complexité.
7. **Rollout Plan** — version cible, déprécations à prévoir, migration
   des consommateurs.

Une RFC incomplète (section manquante) est rejetée d'emblée par les
CODEOWNERS.

---

## 3. Processus — étapes

```
┌──────────────────────────────────────────────────────────────────┐
│  Step 1 : Author ouvre un issue "RFC: <title>"                   │
│           - décrit brièvement le problème et la solution envisagée│
│           - demande un numéro de RFC (NNNN)                      │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Step 2 : Discussion publique dans l'issue pendant 14 jours min. │
│           - les CODEOWNERS participent et orientent la discussion│
│           - l'author incorpore les retours                      │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Step 3 : Author soumet une PR avec le fichier                   │
│           rfcs/NNNN-<slug>.md rempli à partir du template.       │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Step 4 : Review par les CODEOWNERS du module                    │
│           (voir .github/CODEOWNERS — chemin :                    │
│            /AfriOS/AfriOS/OS/afros-corebridge-core/**)           │
│           - au moins 2 approbations requises                     │
│           - le CI valide que API.md / CHANGELOG.md sont cohérents│
│             avec la RFC si elle est acceptée                     │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Step 5 : Décision                                               │
│           - "accepted" : la PR est mergée avec le label          │
│             `rfc-accepted` et un tracking issue est créé pour    │
│             l'implémentation.                                   │
│           - "rejected" : la PR est fermée avec un commentaire    │
│             expliquant pourquoi.                                │
│           - "postponed" : la PR reste ouverte, re-discutée à la  │
│             prochaine révision de roadmap.                      │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Step 6 : Implémentation dans une PR séparée                     │
│           - la PR d'implémentation référence la RFC (ex :        │
│             "Implements RFC 0007")                              │
│           - les changements de headers et de code doivent        │
│             correspondre exactement à la section "Detailed       │
│             Design" de la RFC                                   │
│           - CHANGELOG.md est mis à jour sous [Unreleased]        │
│           - à la release, l'entry est déplacée sous la nouvelle  │
│             version et la RFC est marquée "shipped"              │
└──────────────────────────────────────────────────────────────────┘
```

### 3.1 Délais

| Étape             | Délai minimum                     |
|-------------------|-----------------------------------|
| Discussion (Step 2) | 14 jours calendaires            |
| Review (Step 4)   | 7 jours calendaires               |
| Fast-track (bug critique) | 24 h (voir §5)             |

### 3.2 Numérotation

Les numéros de RFC sont attribués séquentiellement à l'ouverture de
l'issue (Step 1). Une RFC rejetée conserve son numéro (il n'est jamais
réutilisé). La RFC `0000` est réservée au template.

---

## 4. Critères d'acceptation

Une RFC est acceptée si elle satisfait **les cinq critères** suivants :

1. **Compatibilité backward** — la RFC décrit explicitement comment
   l'ancien comportement est préservé (dépréciation, valeur par
   défaut, fallback) ou justifie un breaking change (Tier 1 →
   `MAJOR` obligatoire).
2. **Performance** — la RFC quantifie l'impact performance attendu
   (latence, mémoire, taille binaire). Si un benchmark est possible,
   il est joint à la PR d'implémentation.
3. **Sécurité** — la RFC identifie les surfaces d'attaque nouvelles
   ou modifiées (sandboxing, permissions, accès réseau, parsing de
   données non-trustées) et décrit les mitigations.
4. **Testabilité** — la RFC liste les cas de test à ajouter dans
   `tests/` (unit, integration, compat).
5. **Documentation** — la RFC confirme que `API.md` et
   `CHANGELOG.md` seront mis à jour dans la même PR d'implémentation.

En cas d'échec sur un critère, les CODEOWNERS demandent une
révision avant le merge.

---

## 5. Exceptions

### 5.1 Bug fix critique

Un bug fix qui corrige un crash, une fuite mémoire, ou une faille de
sécurité **peut bypasser le processus RFC** si :

- la signature publique n'est pas modifiée,
- le comportement corrigé était manifestement buggy (un consommateur
  ne pouvait pas dépendre du comportement buggy),
- le fix est documenté **rétroactivement** dans `CHANGELOG.md` sous
  la version qui shipping le fix, avec un renvoi vers une note dans
  `rfcs/` (`rfcs/NNNN-retroactive-bugfix.md`).

Le fast-track (24 h) s'applique alors. Le fix doit être approuvé par
**au moins un** CODEOWNER.

### 5.2 Changement Tier 3

Les APIs Tier 3 (Experimental) peuvent être modifiées ou retirées
sans RFC complète — un commentaire dans la PR d'implémentation et une
entrée dans `CHANGELOG.md` suffisent. C'est le seul cas où le
processus RFC peut être court-circuité volontairement.

---

## 6. Template

Le template officiel est [`rfcs/0000-template.md`](rfcs/0000-template.md).
Pour démarrer une nouvelle RFC :

```sh
cp rfcs/0000-template.md rfcs/0042-my-feature.md
# éditer rfcs/0042-my-feature.md
```

### 6.1 Exemple de RFC remplie

La RFC [`rfcs/0001-stabilize-loader-api.md`](rfcs/0001-stabilize-loader-api.md)
documente rétroactivement la stabilisation de l'API Loader en v1.0.0.
Elle sert d'exemple de format et de niveau de détail attendu.

---

## 7. CODEOWNERS

Le fichier `.github/CODEOWNERS` (à la racine du dépôt `System/`)
doit contenir au minimum :

```
/AfriOS/AfriOS/OS/afros-corebridge-core/**   @ErdisKodjo @afrios/corebridge-maintainers
```

Toute PR modifiant un fichier sous ce chemin requiert l'approbation
d'au moins un de ces owners.

---

## 8. Glossaire

- **Tier 1 / 2 / 3** : voir `API.md` §3.
- **Breaking change** : voir `API.md` §2.1.
- **CODEOWNERS** : fichier GitHub listant les responsables
  par chemin.
- **Fast-track** : processus accéléré pour bug fix critique.
- **Tracking issue** : issue GitHub créé automatiquement après
  acceptation d'une RFC pour suivre son implémentation.

---

*Ce document fait partie du contrat de gouvernance de l'API publique
`afros-corebridge-core`. Toute modification du processus RFC lui-même
doit… suivre le processus RFC (RFC méta).*
