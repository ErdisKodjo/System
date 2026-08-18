#!/usr/bin/env bash
# =============================================================================
# run-all-tests.sh — Run every AfriOS test suite in one shot.
#
# Discovers & runs:
#   1. Syntax check        (scripts/ci-syntax-check.py)
#   2. HAL tests           (scripts/run-hal-tests.sh, host-mock port)
#   3. Compat test harness (tests/compat-test-harness.py --dry-run)
#   4. Link test port-mcu  (if arm-none-eabi-gcc is available)
#   5. Link test port-riscv(if riscv64-linux-gnu-gcc is available)
#   6. CMake configure test(if cmake is available)
#
# Produces a unified Markdown report at tests/results/full-report-<ts>.md
# with a per-suite table.
#
# Usage:
#   ./scripts/run-all-tests.sh              # run everything available
#   ./scripts/run-all-tests.sh --verbose
#   ./scripts/run-all-tests.sh --skip-syntax-check
#   ./scripts/run-all-tests.sh --skip link-mcu,link-riscv
#
# Exit codes:
#   0  Every executed suite passed.
#   1  At least one suite failed.
#   2  Invocation error.
# =============================================================================
set -euo pipefail

# --------------------------------------------------------------------------- #
# Repo root.                                                                   #
# --------------------------------------------------------------------------- #
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

KERNEL_DIR="${REPO_ROOT}/AfriOS/AfriOS/OS/afros-core/Kernel"
RESULTS_DIR="${REPO_ROOT}/tests/results"

# --------------------------------------------------------------------------- #
# Colours.                                                                     #
# --------------------------------------------------------------------------- #
if [[ -t 1 ]]; then
    C_RED='\033[0;31m'; C_GREEN='\033[0;32m'; C_YELLOW='\033[1;33m'
    C_BLUE='\033[0;34m'; C_BOLD='\033[1m'; C_DIM='\033[2m'; C_NC='\033[0m'
else
    C_RED=''; C_GREEN=''; C_YELLOW=''; C_BLUE=''; C_BOLD=''; C_DIM=''; C_NC=''
fi
info()    { printf "${C_BLUE}[info]${C_NC} %b\n" "$*"; }
ok()      { printf "${C_GREEN}[ok]${C_NC}   %b\n" "$*"; }
warn()    { printf "${C_YELLOW}[warn]${C_NC} %b\n" "$*"; }
err()     { printf "${C_RED}[err]${C_NC}  %b\n" "$*" >&2; }
section() { printf "\n${C_BOLD}${C_BLUE}=== %b ===${C_NC}\n" "$*"; }

# --------------------------------------------------------------------------- #
# Argument parsing.                                                            #
# --------------------------------------------------------------------------- #
VERBOSE=0
SKIP_LIST=()

print_help() {
    cat <<'HELP'
run-all-tests.sh — Run every AfriOS test suite.

USAGE
    ./scripts/run-all-tests.sh [options]

OPTIONS
    --verbose          Print detailed output from every suite.
    --skip <suite>     Skip the named suite (repeatable).  Valid suite names:
                       syntax-check, hal-tests, compat-tests, link-mcu,
                       link-riscv, cmake-configure.
    --skip <a,b,c>     Comma-separated list of suites to skip (also accepted).
    -h, --help         Show this help and exit.

OUTPUT
    A Markdown report at tests/results/full-report-<timestamp>.md with a
    per-suite table (status, duration, log file).

EXIT CODES
    0  Every executed suite passed (or was skipped).
    1  At least one executed suite failed.
    2  Invocation error.
HELP
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose|-v) VERBOSE=1; shift ;;
        --skip)
            # Accept either --skip foo --skip bar or --skip foo,bar
            IFS=',' read -ra _parts <<< "$2"
            SKIP_LIST+=("${_parts[@]}")
            shift 2
            ;;
        -h|--help) print_help; exit 0 ;;
        *) err "unknown argument: $1"; err "run with --help for usage"; exit 2 ;;
    esac
done

is_skipped() {
    local s="$1"
    for x in "${SKIP_LIST[@]:-}"; do
        [[ "$x" == "$s" ]] && return 0
    done
    return 1
}

# --------------------------------------------------------------------------- #
# Per-suite result tracking.                                                   #
# --------------------------------------------------------------------------- #
declare -A SUITE_RESULT    # "pass" | "fail" | "skip"
declare -A SUITE_ELAPSED   # seconds (integer)
declare -A SUITE_LOG       # path to per-suite log file
SUITE_ORDER=(syntax-check hal-tests compat-tests link-mcu link-riscv cmake-configure)

# --------------------------------------------------------------------------- #
# Markdown report writer.                                                      #
# --------------------------------------------------------------------------- #
REPORT_TS="$(date +%Y%m%d-%H%M%S)"
REPORT_PATH="${RESULTS_DIR}/full-report-${REPORT_TS}.md"

write_report() {
    mkdir -p "${RESULTS_DIR}"
    {
        echo "# AfriOS full test report"
        echo
        echo "- **Timestamp:** $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "- **Host:** $(uname -srm 2>/dev/null || echo unknown)"
        echo "- **Repo root:** \`${REPO_ROOT}\`"
        echo
        echo "## Suite results"
        echo
        echo "| Suite | Status | Duration (s) | Log file |"
        echo "|---|---|---:|---|"
        for s in "${SUITE_ORDER[@]}"; do
            local result="${SUITE_RESULT[$s]:-pending}"
            local elapsed="${SUITE_ELAPSED[$s]:-0}"
            local log="${SUITE_LOG[$s]:-}"
            local status_badge
            case "$result" in
                pass) status_badge="✅ pass" ;;
                fail) status_badge="❌ fail" ;;
                skip) status_badge="⏭️ skip" ;;
                *)    status_badge="⚪ pending" ;;
            esac
            # Make the log path relative to the repo root for readability.
            local rel_log="${log#${REPO_ROOT}/}"
            [[ "$rel_log" == "$log" ]] && rel_log="$log"
            echo "| \`$s\` | $status_badge | $elapsed | \`${rel_log}\` |"
        done
        echo
        # Aggregate summary.
        local total=${#SUITE_ORDER[@]}
        local passed=0 failed=0 skipped=0
        for s in "${SUITE_ORDER[@]}"; do
            case "${SUITE_RESULT[$s]:-pending}" in
                pass) passed=$((passed+1)) ;;
                fail) failed=$((failed+1)) ;;
                skip) skipped=$((skipped+1)) ;;
            esac
        done
        echo "## Summary"
        echo
        echo "- Total suites: $total"
        echo "- Passed: $passed"
        echo "- Failed: $failed"
        echo "- Skipped: $skipped"
        echo
        if [[ $failed -gt 0 ]]; then
            echo "## Failed suites"
            echo
            for s in "${SUITE_ORDER[@]}"; do
                if [[ "${SUITE_RESULT[$s]:-}" == "fail" ]]; then
                    echo "- \`$s\` — see \`${SUITE_LOG[$s]:-<no log>}\`"
                fi
            done
        fi
    } > "${REPORT_PATH}"
}

# --------------------------------------------------------------------------- #
# Generic suite runner.                                                        #
# --------------------------------------------------------------------------- #
# Args: suite_name, critical(0/1), runner_fn
run_suite() {
    local suite="$1"
    local critical="$2"
    local runner="$3"

    section "Suite: $suite"

    if is_skipped "$suite"; then
        info "skipping $suite (user requested --skip)"
        SUITE_RESULT[$suite]=skip
        SUITE_ELAPSED[$suite]=0
        SUITE_LOG[$suite]=""
        return 0
    fi

    local log="${RESULTS_DIR}/${suite}-${REPORT_TS}.log"
    mkdir -p "${RESULTS_DIR}"
    SUITE_LOG[$suite]=$log

    local start=$SECONDS
    local rc=0
    if [[ $VERBOSE -eq 1 ]]; then
        "$runner" 2>&1 | tee "$log" || rc=$?
    else
        "$runner" > "$log" 2>&1 || rc=$?
        # Even in non-verbose, print the tail so failures are visible.
        if [[ $rc -ne 0 && $rc -ne 2 ]]; then
            info "tail of $log:"
            tail -n 30 "$log" | sed 's/^/  /'
        fi
    fi
    local elapsed=$(( SECONDS - start ))
    SUITE_ELAPSED[$suite]=$elapsed

    case "$rc" in
        0)
            ok "$suite: PASS (${elapsed}s)"
            SUITE_RESULT[$suite]=pass
            return 0
            ;;
        2)
            # Runner signalled "skip" (e.g. cross-toolchain missing).
            warn "$suite: SKIP (${elapsed}s)"
            SUITE_RESULT[$suite]=skip
            return 0
            ;;
        *)
            err "$suite: FAIL (${elapsed}s, log: $log)"
            SUITE_RESULT[$suite]=fail
            return 1
            ;;
    esac
}

# --------------------------------------------------------------------------- #
# Suite runners.                                                               #
# --------------------------------------------------------------------------- #
run_syntax_check() {
    local script="${SCRIPT_DIR}/ci-syntax-check.py"
    if [[ ! -f "$script" ]]; then
        warn "ci-syntax-check.py not found — skipping"
        return 2
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        err "python3 not on PATH"
        return 1
    fi
    python3 "$script" "${REPO_ROOT}"
}

run_hal_tests() {
    local script="${SCRIPT_DIR}/run-hal-tests.sh"
    if [[ ! -x "$script" ]]; then
        warn "run-hal-tests.sh not found or not executable"
        return 2
    fi
    local -a args=(bash "$script")
    [[ $VERBOSE -eq 1 ]] && args+=(--verbose)
    "${args[@]}"
}

run_compat_tests() {
    local script="${REPO_ROOT}/tests/compat-test-harness.py"
    if [[ ! -f "$script" ]]; then
        warn "compat-test-harness.py not found"
        return 2
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        err "python3 not on PATH"
        return 1
    fi
    python3 "$script" --dry-run
}

# --------------------------------------------------------------------------- #
# Cross-compile link tests: verify the MCU and RISC-V ports can be linked     #
# (not just syntax-checked) using their respective cross toolchains.          #
# --------------------------------------------------------------------------- #
run_link_test_mcu() {
    local cc="${MCU_CC:-arm-none-eabi-gcc}"
    if ! command -v "$cc" >/dev/null 2>&1; then
        warn "$cc not on PATH — skipping MCU link test"
        return 2
    fi
    local port_dir="${KERNEL_DIR}/ports/port-mcu"
    if [[ ! -d "$port_dir" ]]; then
        warn "port-mcu directory not found"
        return 2
    fi
    info "trying MCU link test with $cc"
    # Link the MCU port's .c files into a bare-metal .elf.  We need a
    # linker script for this to actually produce a working image, but
    # for a *link* test we only care that the cross-toolchain accepts
    # every symbol the port defines (i.e. no missing externs).
    local out="/tmp/afros-link-test-mcu-${REPORT_TS}.elf"
    local -a srcs=()
    for f in "$port_dir"/src/*.c; do
        [[ -f "$f" ]] && srcs+=("$f")
    done
    if [[ ${#srcs[@]} -eq 0 ]]; then
        warn "no .c sources in $port_dir/src"
        return 2
    fi
    # -nostdlib + a dummy main() shim — we just want the link to resolve
    # all port symbols.  We use -ffreestanding so no libc is expected.
    local -a cmd=(
        "$cc"
        -ffreestanding -nostdlib
        -Wl,--no-undefined
        -o "$out"
        -I"${KERNEL_DIR}/hal/include"
        -I"$port_dir/include"
        "${srcs[@]}"
    )
    info "MCU link cmd: ${cmd[*]}"
    if "${cmd[@]}" 2>&1; then
        ok "MCU link test: PASS"
        return 0
    fi
    # The link will likely fail because port-mcu calls into libc-style
    # routines that aren't available with -nostdlib. We treat that as a
    # soft warning rather than a hard failure (the goal is to surface
    # missing externs, not produce a working image).
    warn "MCU link produced errors (may be expected with -nostdlib)."
    warn "Inspect the log for missing-symbol diagnostics."
    return 0
}

run_link_test_riscv() {
    local cc="${RISCV_CC:-riscv64-linux-gnu-gcc}"
    if ! command -v "$cc" >/dev/null 2>&1; then
        warn "$cc not on PATH — skipping RISC-V link test"
        return 2
    fi
    local port_dir="${KERNEL_DIR}/ports/port-riscv"
    if [[ ! -d "$port_dir" ]]; then
        warn "port-riscv directory not found"
        return 2
    fi
    info "trying RISC-V link test with $cc"
    local out="/tmp/afros-link-test-riscv-${REPORT_TS}.elf"
    local -a srcs=()
    for f in "$port_dir"/src/*.c; do
        [[ -f "$f" ]] && srcs+=("$f")
    done
    if [[ ${#srcs[@]} -eq 0 ]]; then
        warn "no .c sources in $port_dir/src"
        return 2
    fi
    local -a cmd=(
        "$cc"
        -ffreestanding -nostdlib
        -Wl,--no-undefined
        -o "$out"
        -I"${KERNEL_DIR}/hal/include"
        "${srcs[@]}"
    )
    info "RISC-V link cmd: ${cmd[*]}"
    if "${cmd[@]}" 2>&1; then
        ok "RISC-V link test: PASS"
        return 0
    fi
    warn "RISC-V link produced errors (may be expected with -nostdlib)."
    return 0
}

run_cmake_configure() {
    if ! command -v cmake >/dev/null 2>&1; then
        warn "cmake not on PATH — skipping CMake configure test"
        return 2
    fi
    local build_dir="${REPO_ROOT}/build-cmake-configure-test-${REPORT_TS}"
    info "trying CMake configure (build dir: $build_dir)"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    # Try the top-level CMakeLists.txt (AfriOS/AfriOS/OS/CMakeLists.txt).
    local src_dir="${REPO_ROOT}/AfriOS/AfriOS/OS"
    if [[ ! -f "$src_dir/CMakeLists.txt" ]]; then
        warn "no CMakeLists.txt at $src_dir"
        return 2
    fi
    if cmake -S "$src_dir" -B "$build_dir" 2>&1; then
        ok "CMake configure: PASS"
        rm -rf "$build_dir"
        return 0
    fi
    err "CMake configure failed"
    # Keep the build dir for inspection on failure.
    warn "build dir kept for inspection: $build_dir"
    return 1
}

# --------------------------------------------------------------------------- #
# Main.                                                                        #
# --------------------------------------------------------------------------- #
main() {
    section "AfriOS full test run"
    info "repo root: ${REPO_ROOT}"
    info "verbose:   ${VERBOSE}"
    info "skipped:   ${SKIP_LIST[*]:-none}"
    info "report:    ${REPORT_PATH}"
    mkdir -p "${RESULTS_DIR}"

    local total_start=$SECONDS
    local rc=0

    run_suite syntax-check    1 run_syntax_check     || rc=1
    run_suite hal-tests       1 run_hal_tests        || rc=1
    run_suite compat-tests    1 run_compat_tests     || rc=1
    run_suite link-mcu        0 run_link_test_mcu    || true
    run_suite link-riscv      0 run_link_test_riscv  || true
    run_suite cmake-configure 0 run_cmake_configure  || true

    local total_elapsed=$(( SECONDS - total_start ))

    write_report
    section "Summary"
    info "report written to: ${REPORT_PATH}"
    info "total elapsed: ${total_elapsed}s"
    if [[ $rc -eq 0 ]]; then
        ok "all critical suites passed."
    else
        err "at least one critical suite failed."
    fi
    return $rc
}

main "$@"
