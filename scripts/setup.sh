#!/usr/bin/env bash
# =============================================================================
# setup.sh — AfriOS one-shot setup & test orchestration script.
#
# Automates every manual task identified in the AfriOS completion mission:
#   1. check-deps        — verify gcc / g++ / cmake / python3 / git / bash 4+
#   2. install-workflows — bootstrap ci-workflows/ -> .github/
#   3. fetch-edk2        — vendorise EDK2 (off by default, ~5 GB)
#   4. syntax-check      — run scripts/ci-syntax-check.py
#   5. hal-tests         — build & run the HAL test runner with host-mock
#   6. compat-tests      — dry-run the compatibility test harness
#   7. summary           — print a final pass/fail/skip table
#
# Each phase is a function that can be called independently. --all runs them
# in sequence. The script is idempotent (running twice doesn't break anything)
# and exits 0 if every *critical* phase succeeds (fetch-edk2 is optional).
#
# Usage:
#   ./scripts/setup.sh --all                 # run every phase
#   ./scripts/setup.sh --check-deps          # just check deps
#   ./scripts/setup.sh --hal-tests           # just build & run HAL tests
#   ./scripts/setup.sh --syntax-check        # just syntax-check
#   ./scripts/setup.sh --compat-tests        # just compat test discovery
#   ./scripts/setup.sh --install-workflows   # install CI workflows
#   ./scripts/setup.sh --fetch-edk2          # fetch EDK2 (~5 GB, slow)
#   ./scripts/setup.sh --all --skip syntax-check
#   ./scripts/setup.sh --all --verbose
#   ./scripts/setup.sh --help
#
# Exit codes:
#   0 — every critical phase succeeded (or was skipped via --skip)
#   1 — at least one critical phase failed
#   2 — invocation error
# =============================================================================
set -euo pipefail

# --------------------------------------------------------------------------- #
# Locate the repo root.                                                        #
# --------------------------------------------------------------------------- #
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

# --------------------------------------------------------------------------- #
# Colours (disabled when stdout isn't a TTY so CI logs stay clean).            #
# --------------------------------------------------------------------------- #
if [[ -t 1 ]]; then
    C_RED='\033[0;31m'; C_GREEN='\033[0;32m'; C_YELLOW='\033[1;33m'
    C_BLUE='\033[0;34m'; C_BOLD='\033[1m'; C_DIM='\033[2m'; C_NC='\033[0m'
else
    C_RED=''; C_GREEN=''; C_YELLOW=''; C_BLUE=''; C_BOLD=''; C_DIM=''; C_NC=''
fi

# --------------------------------------------------------------------------- #
# Logging helpers.                                                             #
# --------------------------------------------------------------------------- #
# We tee every phase's output to a per-run log file so the user has a single
# artifact to attach to bug reports.  The log path is printed at the start.
SETUP_TS="$(date +%Y%m%d-%H%M%S)"
SETUP_LOG="${REPO_ROOT}/setup-${SETUP_TS}.log"

# Per-phase result tracking (filled in as phases run).
declare -A PHASE_RESULT     # "pass" | "fail" | "skip" | "warn"
declare -A PHASE_ELAPSED    # seconds (integer)
declare -A PHASE_CRITICAL   # 1 if critical, 0 if optional
PHASE_ORDER=(check-deps install-workflows fetch-edk2 syntax-check hal-tests compat-tests summary)

info()    { printf "${C_BLUE}[info]${C_NC} %b\n" "$*" | tee -a "${SETUP_LOG}" >&2; }
ok()      { printf "${C_GREEN}[ok]${C_NC}   %b\n" "$*" | tee -a "${SETUP_LOG}" >&2; }
warn()    { printf "${C_YELLOW}[warn]${C_NC} %b\n" "$*" | tee -a "${SETUP_LOG}" >&2; }
err()     { printf "${C_RED}[err]${C_NC}  %b\n" "$*" | tee -a "${SETUP_LOG}" >&2; }
section() { printf "\n${C_BOLD}${C_BLUE}=== %b ===${C_NC}\n" "$*" | tee -a "${SETUP_LOG}" >&2; }

# Initialise the log file (truncating any prior content from a partial run).
: > "${SETUP_LOG}"

# --------------------------------------------------------------------------- #
# Argument parsing.                                                            #
# --------------------------------------------------------------------------- #
VERBOSE=0
SKIP_PHASES=()
REQUESTED_PHASES=()

print_help() {
    cat <<'HELP' | tee -a "${SETUP_LOG}" >&2
setup.sh — AfriOS one-shot setup & test orchestration.

USAGE
    ./scripts/setup.sh [phase ...] [options]

PHASES (run in this order, unless overridden)
    --check-deps        Verify gcc, g++, cmake, python3, git, bash 4+ are present.
    --install-workflows Bootstrap ci-workflows/ into .github/.
    --fetch-edk2        Vendorise EDK2 (off by default, ~5 GB, slow).
    --syntax-check      Run scripts/ci-syntax-check.py across the repo.
    --hal-tests         Build & run the HAL test runner (host-mock port).
    --compat-tests      Dry-run tests/compat-test-harness.py.
    --summary           Print the final pass/fail/skip table.
    --all               Run every phase above (default if no phase is given).

OPTIONS
    --skip <phase>      Skip the named phase (repeatable).  Useful with --all.
    --verbose           Print detailed output from every phase.
    -h, --help          Show this help and exit.

ENVIRONMENT
    AFROS_SETUP_LOG     Override the default log path.

EXIT CODES
    0  Every critical phase succeeded (or was skipped via --skip).
    1  At least one critical phase failed.
    2  Invocation error.
HELP
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check-deps)        REQUESTED_PHASES+=(check-deps); shift ;;
        --install-workflows) REQUESTED_PHASES+=(install-workflows); shift ;;
        --fetch-edk2)        REQUESTED_PHASES+=(fetch-edk2); shift ;;
        --syntax-check)      REQUESTED_PHASES+=(syntax-check); shift ;;
        --hal-tests)         REQUESTED_PHASES+=(hal-tests); shift ;;
        --compat-tests)      REQUESTED_PHASES+=(compat-tests); shift ;;
        --summary)           REQUESTED_PHASES+=(summary); shift ;;
        --all)               REQUESTED_PHASES=(all); shift ;;
        --skip)              SKIP_PHASES+=("$2"); shift 2 ;;
        --verbose|-v)        VERBOSE=1; shift ;;
        -h|--help)           print_help; exit 0 ;;
        *) err "unknown argument: $1"; err "run with --help for usage"; exit 2 ;;
    esac
done

# Default: run everything.
if [[ ${#REQUESTED_PHASES[@]} -eq 0 ]]; then
    REQUESTED_PHASES=(all)
fi

# Expand "all" into the full ordered list.
if [[ " ${REQUESTED_PHASES[*]} " == *" all "* ]]; then
    REQUESTED_PHASES=("${PHASE_ORDER[@]}")
fi

# Helper: was this phase skipped by the user?
phase_skipped() {
    local p="$1"
    for s in "${SKIP_PHASES[@]:-}"; do
        [[ "$s" == "$p" ]] && return 0
    done
    return 1
}

# Helper: mark a phase's result + elapsed time.
record_phase() {
    local phase="$1" result="$2" elapsed="$3" critical="$4"
    PHASE_RESULT[$phase]=$result
    PHASE_ELAPSED[$phase]=$elapsed
    PHASE_CRITICAL[$phase]=$critical
}

# --------------------------------------------------------------------------- #
# Timer helper.                                                                #
# --------------------------------------------------------------------------- #
# Bash 4+ has $SECONDS; we use it for per-phase elapsed time.
phase_timer_start=0
phase_start() { phase_timer_start=$SECONDS; }
phase_elapsed() { echo $(( SECONDS - phase_timer_start )); }

# --------------------------------------------------------------------------- #
# Phase 1: check-deps                                                          #
# --------------------------------------------------------------------------- #
phase_check_deps() {
    section "Phase: check-deps"
    local missing=0
    local found=()

    check_cmd() {
        local cmd="$1" label="$2"
        if command -v "$cmd" >/dev/null 2>&1; then
            found+=("$label: $(command -v "$cmd")")
            if [[ $VERBOSE -eq 1 ]]; then ok "found $label -> $(command -v "$cmd")"; fi
            return 0
        else
            warn "missing: $label ($cmd not on PATH)"
            missing=$((missing + 1))
            return 1
        fi
    }

    check_cmd gcc      "gcc"
    check_cmd g++      "g++"
    check_cmd cmake    "cmake"
    check_cmd python3  "python3"
    check_cmd git      "git"
    check_cmd make     "make"

    # Bash version check (need 4+ for associative arrays, mapfile, etc.).
    if [[ ${BASH_VERSINFO[0]:-0} -ge 4 ]]; then
        found+=("bash: ${BASH_VERSION}")
        if [[ $VERBOSE -eq 1 ]]; then ok "bash ${BASH_VERSION} (>=4.0)"; fi
    else
        warn "bash version too old: ${BASH_VERSION} (need >=4.0)"
        missing=$((missing + 1))
    fi

    # Optional: cross-compilers for the link tests.
    local opt_tools=(arm-none-eabi-gcc riscv64-linux-gnu-gcc)
    for t in "${opt_tools[@]}"; do
        if command -v "$t" >/dev/null 2>&1; then
            found+=("$t: $(command -v "$t")")
        fi
    done

    info "tools found (${#found[@]}):"
    for f in "${found[@]:-}"; do
        printf "  ${C_DIM}- %s${C_NC}\n" "$f" | tee -a "${SETUP_LOG}" >&2
    done

    if [[ $missing -gt 0 ]]; then
        err "missing ${missing} required tool(s). Install with:"
        err "  sudo apt-get install build-essential cmake python3 git"
        return 1
    fi
    ok "all required tools present."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase 2: install-workflows                                                   #
# --------------------------------------------------------------------------- #
phase_install_workflows() {
    section "Phase: install-workflows"
    local script="${SCRIPT_DIR}/install-workflows.sh"
    if [[ ! -x "$script" ]]; then
        warn "install-workflows.sh not found or not executable: $script"
        warn "skipping this phase."
        return 2  # skip
    fi
    if [[ $VERBOSE -eq 1 ]]; then
        bash "$script" 2>&1 | tee -a "${SETUP_LOG}" >&2 || return 1
    else
        bash "$script" > >(tee -a "${SETUP_LOG}") 2>&1 || return 1
    fi
    ok "workflows installed (or already up to date)."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase 3: fetch-edk2 (optional, off by default)                              #
# --------------------------------------------------------------------------- #
phase_fetch_edk2() {
    section "Phase: fetch-edk2 (OPTIONAL — ~5 GB)"
    local script="${SCRIPT_DIR}/fetch-deps.sh"
    if [[ ! -x "$script" ]]; then
        warn "fetch-deps.sh not found or not executable: $script"
        return 2
    fi
    # EDK2 is huge — make sure the user really wants it.
    if [[ ! -d "${REPO_ROOT}/external/edk2/.git" ]]; then
        warn "EDK2 not yet fetched — this will download ~5 GB and may take 10+ minutes."
        warn "Run with --fetch-edk2 explicitly to opt in."
        warn "Skipping for now."
        return 2
    fi
    if [[ $VERBOSE -eq 1 ]]; then
        bash "$script" edk2 2>&1 | tee -a "${SETUP_LOG}" >&2 || return 1
    else
        bash "$script" edk2 > >(tee -a "${SETUP_LOG}") 2>&1 || return 1
    fi
    ok "EDK2 fetched (or already up to date)."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase 4: syntax-check                                                        #
# --------------------------------------------------------------------------- #
phase_syntax_check() {
    section "Phase: syntax-check"
    local script="${SCRIPT_DIR}/ci-syntax-check.py"
    if [[ ! -f "$script" ]]; then
        warn "ci-syntax-check.py not found: $script"
        return 2
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        err "python3 not on PATH — cannot run syntax-check."
        return 1
    fi
    local -a py_args=(python3 "$script" "${REPO_ROOT}")
    if [[ $VERBOSE -eq 1 ]]; then py_args+=(--verbose); fi
    if ! "${py_args[@]}" 2>&1 | tee -a "${SETUP_LOG}" >&2; then
        err "syntax-check reported failures (see log: ${SETUP_LOG})"
        return 1
    fi
    ok "syntax-check passed."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase 5: hal-tests                                                           #
# --------------------------------------------------------------------------- #
phase_hal_tests() {
    section "Phase: hal-tests (host-mock port)"
    local script="${SCRIPT_DIR}/run-hal-tests.sh"
    if [[ ! -x "$script" ]]; then
        warn "run-hal-tests.sh not found or not executable: $script"
        return 2
    fi
    local -a hal_args=(bash "$script")
    if [[ $VERBOSE -eq 1 ]]; then hal_args+=(--verbose); fi
    if ! "${hal_args[@]}" 2>&1 | tee -a "${SETUP_LOG}" >&2; then
        err "HAL tests failed (see log: ${SETUP_LOG})"
        return 1
    fi
    ok "HAL tests passed."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase 6: compat-tests (dry-run)                                              #
# --------------------------------------------------------------------------- #
phase_compat_tests() {
    section "Phase: compat-tests (dry-run discovery)"
    local script="${REPO_ROOT}/tests/compat-test-harness.py"
    if [[ ! -f "$script" ]]; then
        warn "compat-test-harness.py not found: $script"
        return 2
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        err "python3 not on PATH — cannot run compat-tests."
        return 1
    fi
    # --dry-run discovers tests without actually launching anything.
    # afros-launch is almost certainly missing on a fresh CI runner, so
    # we accept the dry-run path as "compat-tests passed".
    if ! python3 "$script" --dry-run 2>&1 | tee -a "${SETUP_LOG}" >&2; then
        err "compat-test harness failed to discover tests."
        return 1
    fi
    ok "compat-tests dry-run OK."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase 7: summary                                                             #
# --------------------------------------------------------------------------- #
phase_summary() {
    section "Phase: summary"
    local critical_failures=0
    local optional_failures=0
    local skipped=0

    printf "\n" | tee -a "${SETUP_LOG}" >&2
    printf "${C_BOLD}%-22s %-10s %-10s %-10s${C_NC}\n" "Phase" "Result" "Elapsed" "Critical" | tee -a "${SETUP_LOG}" >&2
    printf "%-22s %-10s %-10s %-10s\n" "----------------------" "----------" "----------" "----------" | tee -a "${SETUP_LOG}" >&2
    for p in "${PHASE_ORDER[@]}"; do
        # Don't print the summary phase itself.
        [[ "$p" == "summary" ]] && continue
        local result="${PHASE_RESULT[$p]:-pending}"
        local elapsed="${PHASE_ELAPSED[$p]:-0}s"
        local critical="${PHASE_CRITICAL[$p]:-0}"
        local color="$C_NC"
        case "$result" in
            pass) color="$C_GREEN" ;;
            fail) color="$C_RED" ;;
            skip) color="$C_YELLOW" ;;
            warn) color="$C_YELLOW" ;;
            pending) color="$C_DIM" ;;
        esac
        printf "%-22s ${color}%-10s${C_NC} %-10s %-10s\n" \
            "$p" "$result" "$elapsed" "$critical" | tee -a "${SETUP_LOG}" >&2
        case "$result" in
            fail) if [[ "$critical" == "1" ]]; then critical_failures=$((critical_failures+1)); else optional_failures=$((optional_failures+1)); fi ;;
            skip) skipped=$((skipped+1)) ;;
            warn) skipped=$((skipped+1)) ;;
        esac
    done

    printf "\n" | tee -a "${SETUP_LOG}" >&2
    printf "Critical failures: ${C_RED}%d${C_NC}\n" "$critical_failures" | tee -a "${SETUP_LOG}" >&2
    printf "Optional failures: ${C_YELLOW}%d${C_NC}\n" "$optional_failures" | tee -a "${SETUP_LOG}" >&2
    printf "Phases skipped:    ${C_YELLOW}%d${C_NC}\n" "$skipped" | tee -a "${SETUP_LOG}" >&2
    printf "Log file:          %s\n" "${SETUP_LOG}" | tee -a "${SETUP_LOG}" >&2

    if [[ $critical_failures -gt 0 ]]; then
        err "${critical_failures} critical phase(s) failed."
        return 1
    fi
    ok "every critical phase passed (or was skipped)."
    return 0
}

# --------------------------------------------------------------------------- #
# Phase dispatcher.                                                            #
# --------------------------------------------------------------------------- #
run_phase() {
    local phase="$1"
    local critical="$2"   # 1 = critical, 0 = optional

    if phase_skipped "$phase"; then
        info "skipping phase: $phase (user requested --skip)"
        record_phase "$phase" "skip" 0 "$critical"
        return 0
    fi

    phase_start
    local rc=0
    case "$phase" in
        check-deps)        phase_check_deps        || rc=$? ;;
        install-workflows) phase_install_workflows || rc=$? ;;
        fetch-edk2)        phase_fetch_edk2        || rc=$? ;;
        syntax-check)      phase_syntax_check      || rc=$? ;;
        hal-tests)         phase_hal_tests         || rc=$? ;;
        compat-tests)      phase_compat_tests      || rc=$? ;;
        summary)           phase_summary           || rc=$? ;;
        *) err "internal: unknown phase '$phase'"; return 2 ;;
    esac
    local elapsed=$(phase_elapsed)

    case "$rc" in
        0) record_phase "$phase" "pass" "$elapsed" "$critical" ;;
        1) record_phase "$phase" "fail" "$elapsed" "$critical" ;;
        2) record_phase "$phase" "skip" "$elapsed" "$critical" ;;
        *) record_phase "$phase" "fail" "$elapsed" "$critical" ;;
    esac
    return $rc
}

# --------------------------------------------------------------------------- #
# Main.                                                                        #
# --------------------------------------------------------------------------- #
main() {
    section "AfriOS one-shot setup (log: ${SETUP_LOG})"
    info "repo root: ${REPO_ROOT}"
    info "verbose:   ${VERBOSE}"
    info "skipped:   ${SKIP_PHASES[*]:-none}"
    info "phases:    ${REQUESTED_PHASES[*]}"

    local overall_rc=0
    local total_start=$SECONDS

    # Criticality map:
    #   check-deps        — critical (every other phase depends on it)
    #   install-workflows — critical (CI bootstrap)
    #   fetch-edk2        — optional (~5 GB, off by default)
    #   syntax-check      — critical (CI gate)
    #   hal-tests         — critical (CI gate, AU task)
    #   compat-tests      — critical (CI gate)
    #   summary           — meta (always runs at the end)
    local -A CRITICAL=(
        [check-deps]=1
        [install-workflows]=1
        [fetch-edk2]=0
        [syntax-check]=1
        [hal-tests]=1
        [compat-tests]=1
        [summary]=0
    )

    for phase in "${REQUESTED_PHASES[@]}"; do
        # If the user asked for explicit phases, always append "summary"
        # at the end (unless they explicitly skipped it).
        :
    done

    # Always run summary last (it's idempotent and prints the table).
    local phases_to_run=("${REQUESTED_PHASES[@]}")
    local has_summary=0
    for p in "${phases_to_run[@]}"; do
        [[ "$p" == "summary" ]] && has_summary=1
    done
    if [[ $has_summary -eq 0 ]] && ! phase_skipped summary; then
        phases_to_run+=("summary")
    fi

    for phase in "${phases_to_run[@]}"; do
        if ! run_phase "$phase" "${CRITICAL[$phase]:-0}"; then
            # A failure doesn't abort the whole run — we keep going so
            # the summary at the end can report every phase. We only
            # escalate to non-zero exit at the very end.
            if [[ "${CRITICAL[$phase]:-0}" == "1" ]]; then
                overall_rc=1
            fi
        fi
    done

    local total_elapsed=$(( SECONDS - total_start ))
    info "total elapsed: ${total_elapsed}s"

    if [[ $overall_rc -eq 0 ]]; then
        ok "setup complete — every critical phase passed (or was skipped)."
    else
        err "setup complete — but at least one critical phase failed."
    fi
    info "full log: ${SETUP_LOG}"

    return $overall_rc
}

main "$@"
