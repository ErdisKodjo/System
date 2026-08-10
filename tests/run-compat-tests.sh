#!/bin/bash
# =============================================================================
# run-compat-tests.sh — Orchestrateur principal de la suite de tests de
# compatibilité AfriOS.
#
# Pour chaque plateforme supportée (windows, android, ios, harmonyos,
# linux) :
#   1. Compile les tests sources si nécessaire (Makefile / build.bat).
#   2. Lance le harness Python compat-test-harness.py.
#   3. Collecte les résultats.
# Génère un rapport Markdown final dans tests/results/.
#
# Usage:
#   ./tests/run-compat-tests.sh                  # tout
#   ./tests/run-compat-tests.sh --platform linux
#   ./tests/run-compat-tests.sh --verbose
#   ./tests/run-compat-tests.sh --help
#
# Exit codes:
#   0  tous les tests (non skippés) réussissent
#   1  au moins un test échoue
#   2  erreur d'invocation
# =============================================================================
set -euo pipefail

# ---------------------------------------------------------------------------
# Coloration conditionnelle (désactivée si pas de TTY).
# ---------------------------------------------------------------------------
if [[ -t 1 ]]; then
    C_RED=$'\033[31m'
    C_GREEN=$'\033[32m'
    C_YELLOW=$'\033[33m'
    C_BLUE=$'\033[34m'
    C_RESET=$'\033[0m'
else
    C_RED=""; C_GREEN=""; C_YELLOW=""; C_BLUE=""; C_RESET=""
fi

# ---------------------------------------------------------------------------
# Résolution des chemins (indépendante du CWD).
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXTERNAL_DIR="${REPO_ROOT}/external"
TESTS_DIR="${REPO_ROOT}/tests"
RESULTS_DIR="${TESTS_DIR}/results"

PLATFORMS=(windows android ios harmonyos linux)
PLATFORM=""
VERBOSE=0
JOBS=1

# ---------------------------------------------------------------------------
# Usage.
# ---------------------------------------------------------------------------
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --platform <name>   Limiter à une plateforme
                      (windows|android|ios|harmonyos|linux)
  --verbose           Logs détaillés
  --jobs N            Exécution parallèle (N workers)
  --help              Afficher cette aide

Le script :
  1. Vérifie que external/ est peuplé (sinon suggère ./scripts/fetch-deps.sh)
  2. Compile les tests sources (Makefile) pour chaque plateforme
  3. Lance compat-test-harness.py
  4. Génère tests/results/report-<timestamp>.md
EOF
}

# ---------------------------------------------------------------------------
# Parsing des arguments.
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform)
            [[ -z "${2:-}" ]] && { echo "--platform requires an argument" >&2; exit 2; }
            PLATFORM="$2"
            shift 2
            ;;
        --verbose) VERBOSE=1; shift ;;
        --jobs)
            [[ -z "${2:-}" ]] && { echo "--jobs requires an argument" >&2; exit 2; }
            JOBS="$2"
            shift 2
            ;;
        --help|-h) usage; exit 0 ;;
        *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -n "${PLATFORM}" ]]; then
    valid=0
    for p in "${PLATFORMS[@]}"; do
        [[ "${p}" == "${PLATFORM}" ]] && { valid=1; break; }
    done
    if [[ ${valid} -eq 0 ]]; then
        echo "${C_RED}error:${C_RESET} unknown platform '${PLATFORM}'" >&2
        echo "valid: ${PLATFORMS[*]}" >&2
        exit 2
    fi
    PLATFORMS=("${PLATFORM}")
fi

# ---------------------------------------------------------------------------
# Vérifier que external/ est peuplé. On regarde la présence d'au moins
# un sous-répertoire de dépendance (.git). Si le répertoire est vide ou
# ne contient que des métadonnées, on suggère fetch-deps.sh.
# ---------------------------------------------------------------------------
check_external() {
    if [[ ! -d "${EXTERNAL_DIR}" ]]; then
        echo "${C_YELLOW}warning:${C_RESET} ${EXTERNAL_DIR} n'existe pas." >&2
        echo "  Certains toolchains (mingw, clang cross, d8) peuvent manquer." >&2
        echo "  Suggéré: ./scripts/fetch-deps.sh all" >&2
        return 1
    fi
    local found=0
    while IFS= read -r -d '' subdir; do
        if [[ -d "${subdir}/.git" ]]; then
            found=1
            break
        fi
    done < <(find "${EXTERNAL_DIR}" -mindepth 1 -maxdepth 1 \
                -type d -print0 2>/dev/null || true)
    if [[ ${found} -eq 0 ]]; then
        echo "${C_YELLOW}warning:${C_RESET} external/ est vide ou ne contient" >&2
        echo "  pas de clones. Suggéré: ./scripts/fetch-deps.sh all" >&2
        return 1
    fi
    return 0
}

# ---------------------------------------------------------------------------
# Compilation des tests sources d'une plateforme.
# Cherche tous les Makefile et les invoque si présents.
# ---------------------------------------------------------------------------
build_platform_tests() {
    local platform="$1"
    local plat_dir="${TESTS_DIR}/${platform}"
    [[ -d "${plat_dir}" ]] || return 0

    echo "${C_BLUE}[build]${C_RESET} plateforme=${platform}"
    local found_makefile=0
    while IFS= read -r -d '' mk; do
        found_makefile=1
        local test_dir
        test_dir="$(dirname "${mk}")"
        local test_name
        test_name="$(basename "${test_dir}")"
        if [[ ${VERBOSE} -eq 1 ]]; then
            echo "  make -C ${test_dir}"
        fi
        if ! (cd "${test_dir}" && make 2>&1); then
            echo "${C_YELLOW}  warn:${C_RESET} build échoué pour ${platform}/${test_name}" >&2
            # Non fatal : le harness skippera le test si le binaire manque.
        fi
    done < <(find "${plat_dir}" -mindepth 2 -maxdepth 2 \
                -name Makefile -print0 2>/dev/null || true)

    if [[ ${found_makefile} -eq 0 && ${VERBOSE} -eq 1 ]]; then
        echo "  (aucun Makefile trouvé, bypass)"
    fi
}

# ---------------------------------------------------------------------------
# Lancement du harness Python pour une plateforme.
# ---------------------------------------------------------------------------
run_platform_harness() {
    local platform="$1"
    local args=(python3 "${TESTS_DIR}/compat-test-harness.py"
                --platform "${platform}"
                --jobs "${JOBS}")
    [[ ${VERBOSE} -eq 1 ]] && args+=(--verbose)

    echo "${C_BLUE}[run]${C_RESET}   plateforme=${platform}"
    if ! "${args[@]}"; then
        echo "${C_RED}  fail:${C_RESET} harness a échoué pour ${platform}" >&2
        return 1
    fi
    return 0
}

# ---------------------------------------------------------------------------
# Corps principal.
# ---------------------------------------------------------------------------
main() {
    check_external || true

    mkdir -p "${RESULTS_DIR}"

    local overall_rc=0
    for platform in "${PLATFORMS[@]}"; do
        build_platform_tests "${platform}"
        if ! run_platform_harness "${platform}"; then
            overall_rc=1
        fi
    done

    # Trouver le rapport le plus récent pour l'afficher.
    local latest_report
    latest_report="$(ls -t "${RESULTS_DIR}"/report-*.md 2>/dev/null | head -n1 || true)"
    if [[ -n "${latest_report}" ]]; then
        echo ""
        echo "${C_BLUE}=== Rapport final ===${C_RESET}"
        echo "  ${latest_report}"
        if [[ ${VERBOSE} -eq 1 ]]; then
            echo ""
            cat "${latest_report}"
        fi
    fi

    if [[ ${overall_rc} -eq 0 ]]; then
        echo "${C_GREEN}OK${C_RESET}: tous les tests ont réussi."
    else
        echo "${C_RED}FAIL${C_RESET}: au moins un test a échoué."
    fi
    exit ${overall_rc}
}

main "$@"
