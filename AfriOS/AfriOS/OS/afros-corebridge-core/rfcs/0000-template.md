---
rfc: 0000
title: <Titre court et descriptif>
status: Draft
author: <Votre nom> <vous@example.com>
created: <YYYY-MM-DD>
target_version: <MAJOR.MINOR.PATCH visé, ex : 1.1.0>
tier_impacted: <Tier 1 | Tier 2 | Tier 3>
---

# RFC 0000 — <Titre court et descriptif>

## Summary

<2 à 3 phrases résumant la proposition. Pas de jargon, pas de
justification ici — seulement "quoi". La motivation et les détails
vont dans les sections suivantes.>

Exemple :
> Cette RFC propose d'ajouter une fonction `OrchestratorListRunning()`
> qui retourne la liste des `runtime_handle_t` actuellement actifs.
> La fonction serait Tier 1 Stable à partir de la v1.1.0.

## Motivation

<Décrire le problème que cette RFC résout. Inclure :

- Cas d'usage concret (qui en a besoin, pour quoi faire).
- Limitation actuelle (ce que le caller doit faire aujourd'hui sans
  cette fonction — workaround, hack, absence de fonctionnalité).
- Lien vers des issues / discussions existantes le cas échéant.

2 à 5 paragraphes.>

## Detailed Design

<La partie technique. Inclure :

### Signature proposée

```c
<Type de retour> <Nom de fonction>(<paramètres>);
```

### Headers impactés

- `include/<header>.h` : ajout de la déclaration.
- `include/afros_corebridge.h` : ajout à l'umbrella si Tier 1.

### Structs / enums impactés

```c
typedef struct {
    /* champs existants ... */
    /* nouveau champ : */
    <type> <nouveau_champ>;  /* description */
} <struct>;
```

### Sémantique

- Que fait la fonction, pas à pas ?
- Quels sont les preconditions / postconditions ?
- Quelles sont les valeurs de retour (succès + codes d'erreur) ?
- Quels sont les effets de bord (allocation, lock, thread) ?

### Exemple d'utilisation

```c
/* code compilable montrant comment un caller utilisera la fonction */
```

### Impact sur les sources

Lister les fichiers `.c` à modifier / créer :

- `src/<fichier>.c` : ajout de l'implémentation.
- `tests/<fichier>.c` : tests unitaires.

Au moins 1 paragraphe par sous-section.>

## Alternatives

<Au moins une alternative considérée et rejetée. Pour chaque
alternative :

### Alternative A : <description courte>

- Comment : <comment elle résoudrait le problème>.
- Pourquoi rejetée : <raison concrète — performance, complexité,
  compat, etc.>.

Si vous n'avez pas d'alternative, c'est probablement que vous n'avez
pas assez réfléchi. Au minimum, décrire "ne rien faire" (status quo)
et pourquoi ce n'est pas acceptable.>

## Risks

<Identifier les risques dans les dimensions suivantes. Pour chaque
risque, décrire la mitigation.

### Rétro-compatibilité

- Cette RFC casse-t-elle des callers existants ?
- Si oui, quel est le chemin de migration ?
- Quelle est la politique de dépréciation (voir API.md §2.2) ?

### Performance

- Latence ajoutée (µs / ms) ?
- Mémoire ajoutée (par processus, par handle, par appel) ?
- Taille binaire ajoutée ?
- Si mesurable, joindre un benchmark.

### Sécurité

- Nouvelle surface d'attaque ?
- Parsing de données non-trustées ?
- Élévation de privilège possible ?
- Sandbox / namespace impactés ?

### Complexité

- Lignes de code ajoutées / modifiées.
- Nouveaux modules / fichiers.
- Dépendances introduites.>

## Rollout Plan

<Décrire le déploiement dans le temps.

### Version cible

`v<X.Y.Z>` — Justifier pourquoi cette version (compatibilité avec
d'autres changements, dépendance sur une autre RFC).

### Déprécations

Si la RFC rend obsolète une fonction existante :
- `v<X.Y.0>` : fonction marquée `__attribute__((deprecated))`.
- `v<X.Y+1.0>` : warning runtime.
- `v<X.Y+2.0>` : stub retournant `AFROS_CB_ERR_DEPRECATED`.
- `v<X+1.0.0>` : retrait.

### Migration des consommateurs

- Quels outils / scripts / libraries doivent être mis à jour ?
- Documentation à mettre à jour (README, tutos, exemples).
- Communication (release notes, blog post, annonce Discord).

### Tracking

- Issue de tracking : <numéro à créer après acceptation>.
- PR d'implémentation : <numéro à créer>.
- Tests à ajouter : liste dans `tests/`.
>

## Open Questions

<Lister les questions non résolues qui nécessitent discussion
pendant la période de commentaire (14 jours). Marquer chaque
question comme RESOLVED une fois qu'un consensus émerge.

- Q1 : <question> — RESOLVED : <décision>.
- Q2 : <question> — OPEN.
>

## References

-Liens vers :
- Issues GitHub pertinentes.
- PRs liées.
- Articles / papers / specs externes (RFC POSIX, man pages, etc.).
- Autres RFCs du dépôt (`rfcs/NNNN-*.md`).
>

---

*Remplissez toutes les sections ci-dessus. Supprimez les commentaires
`<...>` avant de soumettre votre PR. Une RFC incomplète sera rejetée
par les CODEOWNERS.*
