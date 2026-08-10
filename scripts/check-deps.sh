#!/bin/bash
# =============================================================================
# check-deps.sh — Verify that AfriOS external dependencies are present and at
# the pinned version.
#
# Walks every dependency declared in fetch-deps.sh, checks:
#   - is external/<name>/ present?            -> OK / MISSING
#   - is its HEAD at the pinned tag/commit?    -> OK / WRONG_VERSION
# Prints a one-line status per dep plus a recap. Exits:
#   0  if every mandatory dep is OK
#   1  if any mandatory dep is MISSING or WRONG_VERSION
#   2  invocation error
#
# Mandatory deps:
#   firmware build : edk2
#   compat layers  : wine, art, darling, harmony, vulkan, glslang
# Optional deps:
#   mesa, iconv
#
# Usage:
#   ./scripts/check-deps.sh            # check everything
#   ./scripts/check-deps.sh --help     # this help
#   EXTERNAL_DIR=/path ./scripts/check-deps.sh
# =============================================================================

set -euo pipefail

EXTERNAL_DIR="${EXTERNAL_DIR:-external}"

# --- Pins (must match fetch-deps.sh) ----------------------------------------
# Duplicated deliberately so check-deps.sh can run even if fetch-deps.sh has
# a syntax error. Keep in sync via scripts/update-dep.sh.
declare -A DEPS=(
    [edk2]="edk2-stable202408"
    [wine]="wine-9.0"
    [art]="android-14.0.0_r1"
    [darling]="0.1.20240301"
    [harmony]="5.0.0-Release"
    [vulkan]="v1.3.290"
    [glslang]="14.2.0"
    [mesa]="mesa-24.0.3"
    [iconv]="v1.17"
)

declare -A SUBDIRS=(
    [edk2]="edk2"
    [wine]="wine"
    [art]="art"
    [darling]="darling"
    [harmony]="harmony-sdk"
    [vulkan]="vulkan-headers"
    [glslang]="glslang"
    [mesa]="mesa"
    [iconv]="libiconv"
)

# Mandatory vs optional classification.
MANDATORY_FIRMWARE=(edk2)
MANDATORY_COMPAT=(wine art darling harmony vulkan glslang)
OPTIONAL=(mesa iconv)

# --- Colours ----------------------------------------------------------------
if [[ -t 1 ]]; then
    C_RED='\033[0;31m'
    C_GREEN='\033[0;32m'
    C_YELLOW='\033[1;33m'
    C_BLUE='\033[0;34m'
    C_BOLD='\033[1m'
    C_NC='\033[0m'
else
    C_RED='' C_GREEN='' C_YELLOW='' C_BLUE='' C_BOLD='' C_NC=''
fi

info()    { printf "${C_BLUE}[info]${C_NC} %b\n"  "$*"; }
ok_msg()  { printf "${C_GREEN}[ok]${C_NC}   %b\n"  "$*"; }
warn()    { printf "${C_YELLOW}[warn]${C_NC} %b\n" "$*"; }
err()     { printf "${C_RED}[err]${C_NC}  %b\n"    "$*" >&2; }
section() { printf "\n${C_BOLD}${C_BLUE}=== %b ===${C_NC}\n" "$*"; }

# --- Help -------------------------------------------------------------------
print_help() {
    cat <<'HELP'
check-deps.sh — Verify AfriOS external dependencies are present & pinned.

USAGE
    ./scripts/check-deps.sh            Check every dependency.
    ./scripts/check-deps.sh --help     Print this help.

ENVIRONMENT
    EXTERNAL_DIR   Override the external/ path (default: ./external)

EXIT CODES
    0   Every mandatory dependency is present and at the pinned version.
    1   At least one mandatory dependency is MISSING or WRONG_VERSION.
    2   Invocation error.

MANDATORY (cause exit 1 if missing/wrong)
    firmware build : edk2
    compat layers  : wine, art, darling, harmony, vulkan, glslang

OPTIONAL (warn only)
    mesa, iconv

SEE ALSO
    scripts/fetch-deps.sh           Fetch missing dependencies
    scripts/fetch-deps-versions.md  Pinned versions & metadata
    scripts/update-dep.sh           Bump a pin
HELP
}

# --- Helpers ----------------------------------------------------------------
is_commit_hash() { [[ "$1" =~ ^[0-9a-f]{40}$ ]]; }

# Returns 0 if the on-disk checkout at $dir matches the pinned version.
checkout_matches_pin() {
    local name="$1" dir="$2"
    local pin="${DEPS[$name]}"
    local head tag
    head=$(git -C "${dir}" rev-parse HEAD 2>/dev/null || true)
    [[ -n "${head}" ]] || return 1
    if is_commit_hash "${pin}"; then
        [[ "${head}" == "${pin}" ]]
    else
        tag=$(git -C "${dir}" describe --tags --exact-match HEAD 2>/dev/null || true)
        [[ "${tag}" == "${pin}" ]]
    fi
}

# --- Per-dep check ----------------------------------------------------------
# Globals: counts (ok_count, missing_count, wrong_count) and the list of
# mandatory deps that failed.
ok_count=0
missing_count=0
wrong_count=0
mandatory_failures=()

# check_dep <name> <kind>  where kind is "mandatory" or "optional"
check_dep() {
    local name="$1" kind="$2"
    local pin="${DEPS[$name]}"
    local subdir="${SUBDIRS[$name]}"
    local dir="${EXTERNAL_DIR}/${subdir}"
    local head tag

    if [[ ! -d "${dir}/.git" ]]; then
        printf "  ${C_RED}MISSING${C_NC}      %-10s (%s, %s) -> %s/\n" \
            "${name}" "${kind}" "${pin}" "${subdir}"
        missing_count=$((missing_count + 1))
        if [[ "${kind}" == "mandatory" ]]; then
            mandatory_failures+=("${name}")
        fi
        return
    fi

    head=$(git -C "${dir}" rev-parse HEAD 2>/dev/null || true)
    if [[ -z "${head}" ]]; then
        printf "  ${C_RED}WRONG_VER${C_NC}   %-10s (%s, %s) HEAD unreadable\n" \
            "${name}" "${kind}" "${pin}"
        wrong_count=$((wrong_count + 1))
        if [[ "${kind}" == "mandatory" ]]; then
            mandatory_failures+=("${name}")
        fi
        return
    fi

    if checkout_matches_pin "${name}" "${dir}"; then
        printf "  ${C_GREEN}OK${C_NC}          %-10s (%s, %s) HEAD=%s\n" \
            "${name}" "${kind}" "${pin}" "${head:0:12}"
        ok_count=$((ok_count + 1))
    else
        # The actual on-disk tag/commit, for the diagnostic.
        if is_commit_hash "${pin}"; then
            tag="${head:0:12}"
        else
            tag=$(git -C "${dir}" describe --tags --exact-match HEAD 2>/dev/null || echo "(detached ${head:0:12})")
        fi
        printf "  ${C_RED}WRONG_VER${C_NC}   %-10s (%s, %s) on-disk=%s\n" \
            "${name}" "${kind}" "${pin}" "${tag}"
        wrong_count=$((wrong_count + 1))
        if [[ "${kind}" == "mandatory" ]]; then
            mandatory_failures+=("${name}")
        fi
    fi
}

# --- Main -------------------------------------------------------------------
main() {
    if [[ $# -gt 0 ]]; then
        case "$1" in
            -h|--help|help)
                print_help
                return 0
                ;;
            *)
                err "unknown argument: $1"
                err "usage: $0 [--help]"
                return 2
                ;;
        esac
    fi

    section "check-deps: ${EXTERNAL_DIR}"

    if [[ ! -d "${EXTERNAL_DIR}" ]]; then
        warn "external dir '${EXTERNAL_DIR}' does not exist"
        warn "run './scripts/fetch-deps.sh all' to populate it."
        # Still walk the deps so the per-dep MISSING lines are printed.
    fi

    printf "\n${C_BOLD}Firmware (mandatory):${C_NC}\n"
    for d in "${MANDATORY_FIRMWARE[@]}"; do
        check_dep "${d}" "mandatory"
    done

    printf "\n${C_BOLD}Compatibility layers (mandatory):${C_NC}\n"
    for d in "${MANDATORY_COMPAT[@]}"; do
        check_dep "${d}" "mandatory"
    done

    printf "\n${C_BOLD}Optional:${C_NC}\n"
    for d in "${OPTIONAL[@]}"; do
        check_dep "${d}" "optional"
    done

    section "check-deps: recap"
    printf "  OK           : %d\n" "${ok_count}"
    printf "  MISSING      : %d\n" "${missing_count}"
    printf "  WRONG_VERSION: %d\n" "${wrong_count}"

    if (( ${#mandatory_failures[@]} > 0 )); then
        printf "  ${C_RED}mandatory failures${C_NC}: %s\n" "${mandatory_failures[*]}"
        err "missing or wrong-version mandatory dependencies: ${mandatory_failures[*]}"
        err "fix with: ./scripts/fetch-deps.sh ${mandatory_failures[0]}"
        return 1
    fi

    if (( wrong_count > 0 || missing_count > 0 )); then
        # Only optional deps failed — warn but exit 0.
        warn "optional deps have issues (OK to ignore if you don't need them)"
    fi
    ok_msg "all mandatory dependencies are present and pinned correctly"
    return 0
}

main "$@"
