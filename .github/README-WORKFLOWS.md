# GitHub Actions Workflows — Statut

Les fichiers `.github/workflows/afrios-ci.yml` et `.github/workflows/afrios-release.yml`
ont été préparés mais ne peuvent pas être commités via le token PAT utilisé pour
ce dépôt (le token manque le scope `workflow` requis par GitHub pour pousser des
fichiers dans `.github/workflows/`).

## Pour activer la CI/CD

Option A — Réutiliser le token avec scope `workflow` :
1. Générer un nouveau PAT avec le scope `workflow` (en plus de `repo`)
2. Cloner le dépôt localement
3. Copier les fichiers depuis `download/github-workflows/` :
   ```bash
   cp -r download/github-workflows/workflows/* .github/workflows/
   cp download/github-workflows/CODEOWNERS .github/
   cp download/github-workflows/pull_request_template.md .github/
   cp -r download/github-workflows/ISSUE_TEMPLATE .github/
   ```
4. Commit + push

Option B — Ajout manuel via l'interface GitHub :
1. Aller sur https://github.com/ErdisKodjo/System/actions/new
2. Coller le contenu de `afrios-ci.yml`
3. Commit

## Fichiers concernés

- `afrios-ci.yml` (678 lignes) — workflow principal avec 7 jobs
- `afrios-release.yml` (321 lignes) — workflow release sur tags v*
- `CODEOWNERS` (93 lignes) — code ownership par sous-système
- `pull_request_template.md` (95 lignes) — template PR
- `ISSUE_TEMPLATE/bug_report.md` + `feature_request.md`

## Note

Les scripts `scripts/ci-syntax-check.py` et `scripts/ci-artifacts-summary.py`
sont bien commités dans le dépôt (ils ne sont pas dans `.github/`). Ils peuvent
être exécutés manuellement :
```bash
python3 scripts/ci-syntax-check.py
```
