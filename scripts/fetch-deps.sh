#!/bin/bash
# =============================================================================
# fetch-deps.sh — Reproducible vendorisation of AfriOS external dependencies.
#
# Clones each external dependency at a pinned tag/commit into external/<name>/,
# using shallow clones to keep disk usage reasonable (some of these repos are
# multiple GB). Idempotent: re-running with an already-correct checkout is a
# no-op. Computes a best-effort SHA-256 of the working tree after clone so two
# developers can verify they ended up with byte-identical sources.
#
# Usage:
#   ./scripts/fetch-deps.sh <name>      # fetch a single dependency
#   ./scripts/fetch-deps.sh all         # fetch every dependency
#   ./scripts/fetch-deps.sh             # print this help
#   ./scripts/fetch-deps.sh --help      # print this help
#
# Environment:
#   EXTERNAL_DIR   Override the destination directory (default: ./external)
#   FETCH_DEPS_SHALLOW
#                  Set to 0 to disable shallow clones (default: 1).
# =============================================================================

set -euo pipefail

# ----------------------------------------------------------------------------
# Strict-mode cleanup: if a clone is interrupted, remove the half-cloned
# directory so the next run doesn't see a corrupt checkout.
# ----------------------------------------------------------------------------
CLEANUP_DIR=""
cleanup() {
    local rc=$?
    if [[ -n "${CLEANUP_DIR}" && -d "${CLEANUP_DIR}" ]]; then
        echo ""
        warn "interrupted while fetching -> removing partial clone at ${CLEANUP_DIR}"
        rm -rf "${CLEANUP_DIR}"
    fi
    exit "${rc}"
}
trap cleanup EXIT INT TERM

# ----------------------------------------------------------------------------
# Configuration: pinned versions, upstream repos, local sub-directory names.
# These pins are the single source of truth for reproducibility — bump them
# here (and in fetch-deps-versions.md) via scripts/update-dep.sh.
# ----------------------------------------------------------------------------
EXTERNAL_DIR="${EXTERNAL_DIR:-external}"
FETCH_DEPS_SHALLOW="${FETCH_DEPS_SHALLOW:-1}"

# Pinned versions (commit hashes or tags). Tags are preferred because they're
# human-readable; commit hashes are acceptable for repos that don't tag.
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

declare -A REPOS=(
    [edk2]="https://github.com/tianocore/edk2.git"
    [wine]="https://gitlab.winehq.org/wine/wine.git"
    [art]="https://android.googlesource.com/platform/art"
    [darling]="https://github.com/darlinghq/darling.git"
    [harmony]="https://gitlab.com/harmonyos/release/ohos-release"
    [vulkan]="https://github.com/KhronosGroup/Vulkan-Headers.git"
    [glslang]="https://github.com/KhronosGroup/glslang.git"
    [mesa]="https://gitlab.freedesktop.org/mesa/mesa.git"
    [iconv]="https://git.savannah.gnu.org/git/libiconv.git"
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

# Order for `all`: smallest first, EDK2 last (~5 GB with submodules).
# This gives the user early feedback and avoids filling the disk before a
# small dependency has a chance to succeed.
ALL_ORDER=(iconv vulkan glslang darling mesa art wine harmony edk2)

# Per-dep extra post-clone steps. Only EDK2 needs submodule init today; we
# declare the hook here so adding more later is one line.
declare -A POST_CLONE_HOOKS=(
    [edk2]="__post_clone_edk2"
)

# ----------------------------------------------------------------------------
# ANSI colours — disabled when stdout is not a TTY (CI logs stay clean).
# ----------------------------------------------------------------------------
if [[ -t 1 ]]; then
    C_RED='\033[0;31m'
    C_GREEN='\033[0;32m'
    C_YELLOW='\033[1;33m'
    C_BLUE='\033[0;34m'
    C_BOLD='\033[1m'
    C_DIM='\033[2m'
    C_NC='\033[0m'
else
    C_RED='' C_GREEN='' C_YELLOW='' C_BLUE='' C_BOLD='' C_DIM='' C_NC=''
fi

info()    { printf "${C_BLUE}[info]${C_NC} %b\n"  "$*"; }
ok()      { printf "${C_GREEN}[ok]${C_NC}   %b\n"  "$*"; }
warn()    { printf "${C_YELLOW}[warn]${C_NC} %b\n" "$*"; }
err()     { printf "${C_RED}[err]${C_NC}  %b\n"    "$*" >&2; }
section() { printf "\n${C_BOLD}${C_BLUE}=== %b ===${C_NC}\n" "$*"; }

# ----------------------------------------------------------------------------
# Help
# ----------------------------------------------------------------------------
print_help() {
    cat <<'HELP'
fetch-deps.sh — Reproducible vendorisation of AfriOS external dependencies.

USAGE
    ./scripts/fetch-deps.sh <name>      Fetch a single dependency.
    ./scripts/fetch-deps.sh all         Fetch every dependency (ordered).
    ./scripts/fetch-deps.sh             Print this help.
    ./scripts/fetch-deps.sh --help      Print this help.

SUPPORTED DEPENDENCIES
    edk2      TianoCore EDK2 (UEFI firmware reference)         ~5 GB
    wine      Wine (Windows compatibility layer)               ~400 MB
    art       Android Runtime (ART)                            ~200 MB
    darling   Darling (macOS/iOS compatibility)                ~100 MB
    harmony   HarmonyOS SDK release                            ~500 MB
    vulkan    Vulkan-Headers (Khronos)                         ~10 MB
    glslang   Khronos GLSL reference compiler                  ~30 MB
    mesa      Mesa 3D graphics library                         ~150 MB
    iconv     GNU libiconv                                     ~5 MB
    all       Fetch every dependency above (smallest first,
              EDK2 last because of its ~5 GB footprint).

ENVIRONMENT
    EXTERNAL_DIR            Destination directory (default: ./external)
    FETCH_DEPS_SHALLOW=0    Disable shallow clones (use full history)

EXAMPLES
    ./scripts/fetch-deps.sh edk2
    ./scripts/fetch-deps.sh all
    EXTERNAL_DIR=/opt/afros-external ./scripts/fetch-deps.sh wine

NOTES
    - Re-running on an already-correct checkout is a no-op (skipped).
    - Shallow clones (--depth 1) are used by default to save bandwidth and
      disk: these upstream trees are large (EDK2 alone is ~5 GB with
      submodules).
    - After each successful clone, a best-effort SHA-256 of the working tree
      (excluding .git/) is computed and printed. Two developers with the same
      pin should get the same digest — record it in fetch-deps-versions.md
      for audit purposes.
    - EDK2 submodules are initialised after the top-level clone
      (git submodule update --init --depth 1).
    - To update a pin, use scripts/update-dep.sh <name> <new-tag-or-commit>.

SEE ALSO
    scripts/fetch-deps-versions.md   Pinned versions & metadata table
    scripts/check-deps.sh            Verify presence and pinned versions
    scripts/update-dep.sh            Bump a pin and re-fetch
    external/README.md               Notes on the external/ directory
HELP
}

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------

# Resolve the local sub-directory path for a dependency name.
dep_dir() { echo "${EXTERNAL_DIR}/${SUBDIRS[$1]}"; }

# Is the pinned version a 40-char hex commit hash?
is_commit_hash() {
    [[ "$1" =~ ^[0-9a-f]{40}$ ]]
}

# Return the HEAD commit of an already-cloned repo, or empty on failure.
local_head() {
    local dir="$1"
    git -C "${dir}" rev-parse HEAD 2>/dev/null || true
}

# Check whether the on-disk checkout at $dir matches the pinned version.
# Returns 0 (match) or 1 (mismatch). Prints nothing.
checkout_matches_pin() {
    local name="$1" dir="$2"
    local pin="${DEPS[$name]}"
    local head tag
    head=$(local_head "${dir}")
    [[ -n "${head}" ]] || return 1
    if is_commit_hash "${pin}"; then
        [[ "${head}" == "${pin}" ]]
    else
        # Tag: rely on `describe --tags --exact-match`. For a shallow clone
        # of --branch <tag>, HEAD is exactly on the tag so this works.
        tag=$(git -C "${dir}" describe --tags --exact-match HEAD 2>/dev/null || true)
        [[ "${tag}" == "${pin}" ]]
    fi
}

# Compute a best-effort SHA-256 of the working tree (excluding .git/).
# Prints the digest on stdout; on failure prints nothing and returns 1.
tree_sha256() {
    local dir="$1"
    (
        cd "${dir}" 2>/dev/null || exit 1
        # Use find -print0 | sort -z | xargs -0 for path-safety. We pipe the
        # list of (path, hash) lines through a final sha256sum so a single
        # digest represents the entire tree state.
        find . -type f -not -path './.git/*' -print0 \
            | sort -z \
            | xargs -0 sha256sum 2>/dev/null \
            | sha256sum \
            | awk '{print $1}'
    ) 2>/dev/null
}

# Human-readable elapsed time from two epoch-second arguments.
elapsed_str() {
    local start="$1" end="$2"
    local diff=$(( end - start ))
    if (( diff < 60 )); then
        printf "%ds" "${diff}"
    elif (( diff < 3600 )); then
        printf "%dm%02ds" $(( diff / 60 )) $(( diff % 60 ))
    else
        printf "%dh%02dm%02ds" $(( diff / 3600 )) $((( diff % 3600 ) / 60 )) $(( diff % 60 ))
    fi
}

# Human-readable disk size of a directory.
dir_size() {
    du -sh "$1" 2>/dev/null | awk '{print $1}'
}

# ----------------------------------------------------------------------------
# Post-clone hooks
# ----------------------------------------------------------------------------

# EDK2 ships a large submodule tree (BaseTools, edk2-platforms, etc.).
# Initialise it shallowly so the firmware build can find the tools.
__post_clone_edk2() {
    local dir="$1"
    info "edk2: initialising submodules (shallow)…"
    git -C "${dir}" submodule update --init --depth 1
}

# ----------------------------------------------------------------------------
# Core fetch logic for a single dependency.
# Returns 0 on success (or skip), non-zero on hard failure.
# ----------------------------------------------------------------------------
fetch_one() {
    local name="$1"

    if [[ -z "${DEPS[$name]:-}" ]]; then
        err "unknown dependency: ${name}"
        err "valid names: ${!DEPS[*]} all"
        return 2
    fi

    local pin="${DEPS[$name]}"
    local repo="${REPOS[$name]}"
    local subdir="${SUBDIRS[$name]}"
    local dir="${EXTERNAL_DIR}/${subdir}"
    local start end

    section "fetch-deps: ${name} (pin=${pin})"

    # Skip case: already cloned and at the right commit/tag.
    if [[ -d "${dir}/.git" ]]; then
        if checkout_matches_pin "${name}" "${dir}"; then
            local head
            head=$(local_head "${dir}")
            ok "${name}: déjà à jour (HEAD=${head:0:12}, pin=${pin})"
            local sha
            sha=$(tree_sha256 "${dir}")
            [[ -n "${sha}" ]] && info "tree sha256: ${sha}"
            info "size on disk: $(dir_size "${dir}")"
            return 0
        else
            warn "${name}: checkout exists but does not match pin '${pin}' — re-cloning"
            rm -rf "${dir}"
        fi
    fi

    # Clone case.
    info "${name}: cloning ${repo} (pin=${pin})…"
    mkdir -p "${EXTERNAL_DIR}"
    CLEANUP_DIR="${dir}"
    start=$(date +%s)

    local clone_args=(clone --quiet)
    if [[ "${FETCH_DEPS_SHALLOW}" == "1" ]]; then
        clone_args+=(--depth 1 --branch "${pin}")
    else
        # Non-shallow: we still need to fetch the specific ref. Use --branch
        # which accepts tags or branch names.
        clone_args+=(--branch "${pin}")
    fi
    clone_args+=("${repo}" "${dir}")

    if ! git "${clone_args[@]}"; then
        err "${name}: git clone failed (pin=${pin}, repo=${repo})"
        err "  if this is a transient network error, just re-run."
        err "  if the tag/commit is wrong, edit scripts/fetch-deps.sh."
        return 1
    fi

    # Run the per-dep post-clone hook if registered.
    local hook="${POST_CLONE_HOOKS[$name]:-}"
    if [[ -n "${hook}" ]] && declare -F "${hook}" >/dev/null; then
        "${hook}" "${dir}"
    fi

    end=$(date +%s)
    CLEANUP_DIR=""   # success — don't let the trap remove it

    local head
    head=$(local_head "${dir}")
    ok "${name}: fetched in $(elapsed_str "${start}" "${end}")" \
       "(HEAD=${head:0:12}, pin=${pin})"

    # Best-effort SHA-256. Don't fail the whole run if it can't be computed
    # (e.g. on hosts without `xargs`).
    local sha
    sha=$(tree_sha256 "${dir}")
    if [[ -n "${sha}" ]]; then
        info "tree sha256: ${sha}"
    else
        warn "${name}: could not compute tree SHA-256 (best-effort, ignoring)"
    fi
    info "size on disk: $(dir_size "${dir}")"

    return 0
}

# ----------------------------------------------------------------------------
# `all` — iterate in ALL_ORDER, accumulate failures, print a recap.
# ----------------------------------------------------------------------------
fetch_all() {
    section "fetch-deps: ALL (${#ALL_ORDER[@]} dependencies, smallest first)"
    mkdir -p "${EXTERNAL_DIR}"

    local -a succeeded=()
    local -a failed=()
    local -a skipped=()
    local name rc start end
    start=$(date +%s)

    for name in "${ALL_ORDER[@]}"; do
        if fetch_one "${name}"; then
            # Distinguish "actually cloned" from "skipped (already up to date)"
            # by checking the post-state — but fetch_one already printed the
            # appropriate message, so we just record success here.
            succeeded+=("${name}")
        else
            rc=$?
            if (( rc == 2 )); then
                # Unknown dep — shouldn't happen for `all`, but be safe.
                failed+=("${name}")
            else
                failed+=("${name}")
            fi
        fi
    done

    end=$(date +%s)

    section "fetch-deps: recap"
    printf "  total elapsed : %s\n" "$(elapsed_str "${start}" "${end}")"
    printf "  succeeded     : %d (%s)\n" "${#succeeded[@]}" "${succeeded[*]:-}"
    if (( ${#failed[@]} > 0 )); then
        printf "  ${C_RED}failed${C_NC}        : %d (%s)\n" "${#failed[@]}" "${failed[*]}"
    else
        printf "  failed        : 0\n"
    fi
    printf "  external dir  : %s (%s on disk)\n" \
        "${EXTERNAL_DIR}" "$(dir_size "${EXTERNAL_DIR}")"

    if (( ${#failed[@]} > 0 )); then
        err "one or more dependencies failed to fetch"
        return 1
    fi
    ok "all dependencies fetched successfully"
    return 0
}

# ----------------------------------------------------------------------------
# Entry point
# ----------------------------------------------------------------------------
main() {
    # No argument → help.
    if [[ $# -eq 0 ]]; then
        print_help
        return 0
    fi

    case "$1" in
        -h|--help|help)
            print_help
            return 0
            ;;
        all)
            fetch_all
            ;;
        edk2|wine|art|darling|harmony|vulkan|glslang|mesa|iconv)
            fetch_one "$1"
            ;;
        *)
            err "unknown dependency: $1"
            err "valid names: ${!DEPS[*]} all"
            err "run './scripts/fetch-deps.sh --help' for the full list."
            return 2
            ;;
    esac
}

main "$@"
