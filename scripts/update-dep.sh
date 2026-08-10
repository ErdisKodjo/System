#!/bin/bash
# =============================================================================
# update-dep.sh — Bump a pinned external dependency and re-fetch it.
#
# Workflow:
#   1. Validate <name> and <new-tag-or-commit>.
#   2. Read the current pin from scripts/fetch-deps.sh.
#   3. Rewrite the pin line in scripts/fetch-deps.sh.
#   4. Update the version cell in scripts/fetch-deps-versions.md.
#   5. Re-fetch the dependency (./scripts/fetch-deps.sh <name>) to validate
#      the new tag/commit actually resolves upstream.
#   6. If the re-fetch fails, revert steps 3–4 and exit non-zero.
#   7. On success, print a unified diff of scripts/fetch-deps.sh and a
#      suggested commit command. The script does NOT commit — that's the
#      user's call.
#
# Usage:
#   ./scripts/update-dep.sh <name> <new-tag-or-commit>
#   ./scripts/update-dep.sh --help
#
# Example:
#   ./scripts/update-dep.sh wine wine-9.1
# =============================================================================

set -euo pipefail

# Resolve repo-relative paths regardless of where the script is invoked from.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FETCH_DEPS_SH="${SCRIPT_DIR}/fetch-deps.sh"
VERSIONS_MD="${SCRIPT_DIR}/fetch-deps-versions.md"

# --- Colours ----------------------------------------------------------------
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

# --- Help -------------------------------------------------------------------
print_help() {
    cat <<'HELP'
update-dep.sh — Bump a pinned AfriOS external dependency.

USAGE
    ./scripts/update-dep.sh <name> <new-tag-or-commit>
    ./scripts/update-dep.sh --help

ARGUMENTS
    name                  One of: edk2, wine, art, darling, harmony, vulkan,
                          glslang, mesa, iconv
    new-tag-or-commit     The new pin. May be a tag (e.g. wine-9.1) or a
                          40-char commit hash.

WHAT IT DOES
    1. Rewrites the pin in scripts/fetch-deps.sh (the DEPS array).
    2. Updates the version cell in scripts/fetch-deps-versions.md.
    3. Re-fetches the dependency to validate the new pin resolves upstream.
       If the re-fetch fails, both files are reverted and the script exits 1.
    4. Prints a unified diff of scripts/fetch-deps.sh.
    5. Prints a suggested `git commit` command. The script does NOT commit.

EXAMPLES
    ./scripts/update-dep.sh wine wine-9.1
    ./scripts/update-dep.sh edk2 edk2-stable202411
    ./scripts/update-dep.sh vulkan a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2

SEE ALSO
    scripts/fetch-deps.sh           The fetcher (source of truth for pins)
    scripts/fetch-deps-versions.md  Pinned versions & metadata table
    scripts/check-deps.sh           Verify presence + pinned versions
HELP
}

# --- Helpers ----------------------------------------------------------------

# Escape ERE metacharacters so a string is matched literally by awk/sed -E.
ere_escape() {
    printf '%s' "$1" | sed 's/[.[\*^$()+?{|\\]/\\&/g'
}

# Extract the current pin for a dep name from fetch-deps.sh by sourcing the
# DEPS array. We can't source the whole script (it calls main), so we extract
# the DEPS block with sed and eval just that.
read_current_pin() {
    local name="$1"
    # Pull out the `declare -A DEPS=( ... )` block, then look up the name.
    # Using awk to grab the block between `declare -A DEPS=(` and the
    # matching `)`.
    local block
    block=$(awk '
        /^declare -A DEPS=\(/ { in_block=1; next }
        in_block && /^\)/     { in_block=0; next }
        in_block               { print }
    ' "${FETCH_DEPS_SH}")
    # Now eval the block as `declare -A DEPS=( ... )` in a subshell and
    # print the value for $name.
    (
        # shellcheck disable=SC1090
        eval "declare -A DEPS=( ${block} )"
        printf '%s' "${DEPS[$name]:-}"
    )
}

# Rewrite the pin line for $name in fetch-deps.sh. Returns 0 on success.
# Writes the modified content back through the existing inode (via `cat >`)
# so the file's permissions are preserved — `mv` would replace it with a
# 0600-mode tempfile, breaking `./scripts/fetch-deps.sh` execution.
rewrite_pin_in_fetch_deps() {
    local name="$1" new_pin="$2"
    local tmp
    tmp=$(mktemp)
    # Match:  <whitespace>[<name>]="<anything>"
    # Replace with: <whitespace>[<name>]="<new_pin>"
    # -E for extended regex; the name is a simple identifier so no escaping
    # is needed for it; the new pin is interpolated into the replacement,
    # where only & / \ are special — escape them.
    local escaped_new
    escaped_new=$(printf '%s' "${new_pin}" | sed 's/[&\\]/\\&/g')
    # Apply the substitution ONLY within the `declare -A DEPS=( ... )` block
    # — otherwise the same `[<name>]=` pattern would also match the REPOS
    # and SUBDIRS arrays (which use the same key names) and clobber the
    # upstream URL / subdirectory name. The address range
    # `/^declare -A DEPS=(/,/^)/` restricts the `s` command to that block.
    if sed -E "/^declare -A DEPS=\\(/,/^\\)/ s/^([[:space:]]*\\[${name}\\]=)\"[^\"]*\"/\\1\"${escaped_new}\"/" \
            "${FETCH_DEPS_SH}" > "${tmp}"; then
        # Verify the replacement happened *and* didn't leak into REPOS/SUBDIRS.
        # Extract just the DEPS block from the candidate file and confirm the
        # new pin is on the <name> line there.
        local dep_line needle
        needle="\"${new_pin}\""
        dep_line=$(awk '
            /^declare -A DEPS=\(/ { in_block=1; next }
            in_block && /^\)/     { in_block=0; next }
            in_block && $0 ~ "^[[:space:]]*\\['"${name}"'\\]=" { print; exit }
        ' "${tmp}")
        # case-glob is more robust than [[ == *pattern* ]] for strings
        # containing quote characters.
        case "${dep_line}" in
            *"${needle}"*)
                cat "${tmp}" > "${FETCH_DEPS_SH}"
                rm -f "${tmp}"
                return 0
                ;;
        esac
    fi
    rm -f "${tmp}"
    return 1
}

# Update the version cell in fetch-deps-versions.md for $name.
# The row looks like:  | `name` | `old-pin` | date | ...
# We replace the first occurrence of `old-pin` on that row with `new-pin`.
update_version_cell() {
    local name="$1" old_pin="$2" new_pin="$3"
    local old_ere row_pat tmp
    old_ere=$(ere_escape "${old_pin}")
    row_pat=$(ere_escape "${name}")
    tmp=$(mktemp)
    awk -v row="| \`${name}\`" -v old="${old_ere}" -v new="${new_pin}" '
        index($0, row) > 0 {
            # Replace the first occurrence of `old_pin` on this row.
            # awk sub() uses ERE; old is already escaped.
            sub("`" old "`", "`" new "`")
        }
        { print }
    ' "${VERSIONS_MD}" > "${tmp}" && cat "${tmp}" > "${VERSIONS_MD}" && rm -f "${tmp}"
}

# Restore fetch-deps.sh and versions.md from backups.
# Uses `cat >` (not `mv`) so the destination file's inode and mode are
# preserved — `mv` would clobber the executable bit on scripts/fetch-deps.sh.
restore_backups() {
    if [[ -f "${FETCH_DEPS_SH}.bak" ]]; then
        cat "${FETCH_DEPS_SH}.bak" > "${FETCH_DEPS_SH}"
        rm -f "${FETCH_DEPS_SH}.bak"
    fi
    if [[ -f "${VERSIONS_MD}.bak" ]]; then
        cat "${VERSIONS_MD}.bak" > "${VERSIONS_MD}"
        rm -f "${VERSIONS_MD}.bak"
    fi
}

# --- Main -------------------------------------------------------------------
main() {
    if [[ $# -eq 1 && "$1" == "-h" ]]; then
        print_help; return 0
    fi
    if [[ $# -eq 1 && ("$1" == "--help" || "$1" == "help") ]]; then
        print_help; return 0
    fi
    if [[ $# -ne 2 ]]; then
        err "usage: $0 <name> <new-tag-or-commit>"
        err "run '$0 --help' for details."
        return 2
    fi

    local name="$1" new_pin="$2"

    # Validate name against the known dep list (re-read from fetch-deps.sh
    # so we don't drift).
    local known_pins
    known_pins=$(
        awk '
            /^declare -A DEPS=\(/ { in_block=1; next }
            in_block && /^\)/     { in_block=0; next }
            in_block               { sub(/^[[:space:]]*\[/,""); sub(/\].*/,""); print }
        ' "${FETCH_DEPS_SH}"
    )
    if ! grep -qx "${name}" <<<"${known_pins}"; then
        err "unknown dependency: ${name}"
        err "known names: $(echo "${known_pins}" | tr '\n' ' ')"
        return 2
    fi

    local old_pin
    old_pin=$(read_current_pin "${name}")
    if [[ -z "${old_pin}" ]]; then
        err "could not read current pin for '${name}' from ${FETCH_DEPS_SH}"
        return 1
    fi

    if [[ "${old_pin}" == "${new_pin}" ]]; then
        warn "${name} is already pinned to '${new_pin}' — nothing to do."
        return 0
    fi

    section "update-dep: ${name}  ${old_pin} -> ${new_pin}"

    # Backups so we can revert on failure.
    cp "${FETCH_DEPS_SH}" "${FETCH_DEPS_SH}.bak"
    cp "${VERSIONS_MD}"   "${VERSIONS_MD}.bak"
    trap 'restore_backups; err "interrupted — reverted pin files." ' INT TERM

    # 1. Rewrite the pin in fetch-deps.sh.
    info "rewriting pin in scripts/fetch-deps.sh…"
    if ! rewrite_pin_in_fetch_deps "${name}" "${new_pin}"; then
        restore_backups
        err "failed to rewrite pin line for '${name}' in ${FETCH_DEPS_SH}"
        err "(is the DEPS array format unchanged?)"
        return 1
    fi
    ok "pin updated in scripts/fetch-deps.sh"

    # 2. Update the version cell in fetch-deps-versions.md.
    info "updating version cell in scripts/fetch-deps-versions.md…"
    if ! update_version_cell "${name}" "${old_pin}" "${new_pin}"; then
        restore_backups
        err "failed to update ${VERSIONS_MD}"
        return 1
    fi
    ok "version cell updated in scripts/fetch-deps-versions.md"
    warn "remember to update the 'Tested with AfriOS commit' column for ${name}"
    warn "after the next green CI run on the relevant job."

    # 3. Re-fetch to validate the new pin resolves upstream.
    section "re-fetching ${name} to validate the new pin"
    info "running: ./scripts/fetch-deps.sh ${name}"
    if ! ( cd "${REPO_ROOT}" && ./scripts/fetch-deps.sh "${name}" ); then
        err "re-fetch failed — reverting pin files."
        err "the existing external/ checkout for ${name} may have been wiped;"
        err "re-run './scripts/fetch-deps.sh ${name}' after fixing the pin."
        restore_backups
        trap - INT TERM
        return 1
    fi
    ok "re-fetch succeeded — new pin '${new_pin}' is valid upstream."

    # 4. Show the diff of fetch-deps.sh.
    section "diff: scripts/fetch-deps.sh"
    if diff -u "${FETCH_DEPS_SH}.bak" "${FETCH_DEPS_SH}"; then
        :
    fi
    # Also a one-line summary of the versions.md change.
    section "diff: scripts/fetch-deps-versions.md (summary)"
    if diff -u "${VERSIONS_MD}.bak" "${VERSIONS_MD}" | head -20; then
        :
    fi

    # 5. Clean up backups.
    rm -f "${FETCH_DEPS_SH}.bak" "${VERSIONS_MD}.bak"
    trap - INT TERM

    # 6. Suggested commit command (do NOT run it).
    section "next steps"
    cat <<EOF
${C_GREEN}The pin has been bumped and the dependency re-fetched successfully.${C_NC}

Suggested commit (review the diff above first):

  ${C_BOLD}git add scripts/fetch-deps.sh scripts/fetch-deps-versions.md${C_NC}
  ${C_BOLD}git commit -m "deps: bump ${name} from ${old_pin} to ${new_pin}"${C_NC}

Then push and run the relevant CI job:
  - firmware-build   for edk2
  - compat-layer-build for wine, art, darling, harmony, vulkan, glslang

On a green CI run, update the 'Tested with AfriOS commit' cell in
scripts/fetch-deps-versions.md with the AfriOS commit SHA that built
successfully against this new pin.
EOF
    return 0
}

main "$@"
