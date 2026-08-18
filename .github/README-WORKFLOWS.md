# GitHub Actions Workflows — Statut

Les fichiers `.github/workflows/afrios-ci.yml` et
`.github/workflows/afrios-release.yml` sont **prêts** mais ne peuvent pas
être commités via le token PAT historiquement utilisé pour ce dépôt (le
token manque le scope `workflow` requis par GitHub pour pousser des
fichiers dans `.github/workflows/`).

Pour contourner cette limitation, **les copies canoniques de tous les
fichiers CI/CD vivent dans `ci-workflows/`** (qui est un répertoire
ordinaire, librement poussable). Un script d'installation les recopie
dans `.github/` quand un token correctement scoppé est disponible.

## Layout

```
ci-workflows/
├── afrios-ci.yml               # workflow principal (7 jobs)
├── afrios-release.yml          # workflow release sur tags v*
├── CODEOWNERS                  # code ownership par sous-système
├── pull_request_template.md    # template PR
└── ISSUE_TEMPLATE/
    ├── bug_report.md
    └── feature_request.md
```

## Pour activer la CI/CD

### Option A — Script d'installation (recommandé)

```bash
# 1. Obtenir un PAT avec le scope `workflow` (en plus de `repo`).
# 2. Depuis une copie locale du dépôt :
bash scripts/install-workflows.sh
# → copie ci-workflows/* vers .github/, et `git add` le résultat.
# 3. Commit + push :
git commit -m "ci: bootstrap GitHub Actions workflows"
git push origin main
```

Pour tenter un push automatique (si `GH_TOKEN` a le scope `workflow`) :

```bash
GH_TOKEN=<pat-avec-scope-workflow> bash scripts/install-workflows.sh
```

Le script :
- Vérifie si `.github/workflows/` a déjà les fichiers (skip si identiques).
- Copie depuis `ci-workflows/` vers `.github/` (workflows, CODEOWNERS,
  PR template, ISSUE_TEMPLATE/).
- `git add --force` tout ce qui a été copié.
- Si `GH_TOKEN` est présent, tente un `git push` automatique.
- N'appelle **jamais** `git commit` sans `GH_TOKEN` (le protocole multi-agent
  exige que le commit soit fait par l'agent parent ou par un humain).

### Option B — Ajout manuel via l'interface GitHub

1. Aller sur https://github.com/ErdisKodjo/System/actions/new
2. Coller le contenu de `ci-workflows/afrios-ci.yml`
3. Commit (l'UI GitHub a le scope `workflow` par défaut)

## Fichiers concernés

| Fichier                              | Lignes | Rôle                                 |
| ---                                  | ---    | ---                                  |
| `ci-workflows/afrios-ci.yml`         | 678    | workflow CI principal (7 jobs)       |
| `ci-workflows/afrios-release.yml`    | 321    | workflow release sur tags `v*`       |
| `ci-workflows/CODEOWNERS`            | 93     | code ownership par sous-système      |
| `ci-workflows/pull_request_template.md` | 95  | template PR                          |
| `ci-workflows/ISSUE_TEMPLATE/bug_report.md`     | 83 | template bug report       |
| `ci-workflows/ISSUE_TEMPLATE/feature_request.md` | 77 | template feature request |

## Note

Les scripts `scripts/ci-syntax-check.py` et
`scripts/ci-artifacts-summary.py` sont bien commités dans le dépôt (ils
ne sont pas dans `.github/`). Ils peuvent être exécutés manuellement :

```bash
python3 scripts/ci-syntax-check.py
```

## Voir aussi

- `scripts/install-workflows.sh` — le bootstrap qui copie `ci-workflows/`
  vers `.github/` et stage le résultat dans git.
