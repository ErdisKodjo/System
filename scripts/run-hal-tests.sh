#!/usr/bin/env bash
# =============================================================================
# run-hal-tests.sh — Build & run the AfriOS HAL test runner.
#
# Detects whether we're on a Linux host (use the host-mock port — no
# bare-metal required) or in a freestanding / cross-compiled environment
# (use the real port). Builds hal_test_runner, runs it, captures output to
# tests/results/hal-tests-<timestamp>.log, and exits 0 on success / 1 on
# failure.
#
# The script tries CMake first (if a build/ dir is configured) and falls
# back to a direct gcc invocation if CMake isn't available or the build/
# dir doesn't exist. The direct-gcc path is what CI runners use because
# the project doesn't ship a top-level CMakeLists.txt that wires up the
# HAL tests on its own.
#
# Usage:
#   scripts/run-hal-tests.sh                 # auto-detect port, build, run
#   scripts/run-hal-tests.sh --port host-mock
#   scripts/run-hal-tests.sh --port x86_64   # will fail on host (privileged
#                                            # insns), use only on bare-metal
#   scripts/run-hal-tests.sh --verbose
#   scripts/run-hal-tests.sh --no-cmake      # force direct gcc path
#
# Exit codes:
#   0  — all HAL tests passed
#   1  — at least one test failed (or build failed)
#   2  — invocation error
# =============================================================================
set -euo pipefail

# --------------------------------------------------------------------------- #
# Locate the repo root (parent of this scripts/ dir).                         #
# --------------------------------------------------------------------------- #
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

# HAL source layout under the afros-core kernel tree.
KERNEL_DIR="${REPO_ROOT}/AfriOS/AfriOS/OS/afros-core/Kernel"
HAL_DIR="${KERNEL_DIR}/hal"
HAL_INCLUDE="${HAL_DIR}/include"
HAL_SRC="${HAL_DIR}/src"
TESTS_DIR="${KERNEL_DIR}/hal/tests"
PORTS_DIR="${KERNEL_DIR}/ports"
HOST_MOCK_DIR="${PORTS_DIR}/port-host-mock"
HOST_MOCK_INCLUDE="${HOST_MOCK_DIR}/include"
HOST_MOCK_SRC="${HOST_MOCK_DIR}/src"
RESULTS_DIR="${REPO_ROOT}/tests/results"

# --------------------------------------------------------------------------- #
# Defaults overridable via flags / env.                                       #
# --------------------------------------------------------------------------- #
PORT=""            # auto-detect
VERBOSE=0
FORCE_NO_CMAKE=0
EXTRA_CFLAGS=()
EXTRA_LDFLAGS=()

# Colours (disabled when not a TTY so CI logs stay clean).
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
# Argument parsing.                                                           #
# --------------------------------------------------------------------------- #
print_help() {
    cat <<'HELP'
run-hal-tests.sh — Build & run the AfriOS HAL test runner.

USAGE
    scripts/run-hal-tests.sh [options]

OPTIONS
    --port <name>     Force a specific port (host-mock, x86_64, arm64, riscv, mcu).
                      Default: auto-detect (host-mock on Linux host).
    --verbose         Print every gcc command and full test output.
    --no-cmake        Skip the CMake path, force direct gcc invocation.
    --extra-cflag F   Append F to the gcc compile flags (repeatable).
    --extra-ldflag F  Append F to the gcc link flags (repeatable).
    -h, --help        Show this help.

ENVIRONMENT
    AFROS_HAL_TEST_PORT   Override the default port (same as --port).
    AFROS_HAL_BUILD_DIR   Use this CMake build directory if it exists.

EXIT CODES
    0  All HAL tests passed.
    1  Build failed, or at least one test failed.
    2  Invocation error.
HELP
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)         PORT="$2"; shift 2 ;;
        --verbose|-v)   VERBOSE=1; shift ;;
        --no-cmake)     FORCE_NO_CMAKE=1; shift ;;
        --extra-cflag)  EXTRA_CFLAGS+=("$2"); shift 2 ;;
        --extra-ldflag) EXTRA_LDFLAGS+=("$2"); shift 2 ;;
        -h|--help)      print_help; exit 0 ;;
        *) err "unknown argument: $1"; err "run with --help for usage"; exit 2 ;;
    esac
done

# Env override.
if [[ -z "${PORT}" && -n "${AFROS_HAL_TEST_PORT:-}" ]]; then
    PORT="${AFROS_HAL_TEST_PORT}"
fi

# --------------------------------------------------------------------------- #
# Port detection.                                                             #
# --------------------------------------------------------------------------- #
detect_port() {
    # If the user told us which port to use, honour it.
    if [[ -n "${PORT}" ]]; then
        return
    fi

    # Default: on a Linux host (where this script is normally invoked), use
    # the host-mock port — that's the whole point of this script (running
    # HAL tests without bare-metal). On bare-metal / cross-compile builds
    # the caller should pass --port explicitly.
    local uname_s
    uname_s="$(uname -s 2>/dev/null || echo unknown)"
    case "${uname_s}" in
        Linux)
            PORT="host-mock"
            ;;
        Darwin)
            # macOS host: host-mock mostly works (it uses POSIX), but the
            # storage backing path /tmp/... is fine. Treat as host-mock too.
            PORT="host-mock"
            ;;
        *)
            warn "unknown OS '${uname_s}' — defaulting to host-mock"
            PORT="host-mock"
            ;;
    esac
}

detect_port
info "selected port: ${PORT}"

if [[ "${PORT}" != "host-mock" ]]; then
    warn "port '${PORT}' requires a real (bare-metal or QEMU) target."
    warn "this script only builds the host-mock path on a Linux host."
    warn "for other ports, use the cross-compile toolchain manually."
    err "only --port host-mock is supported by this script."
    exit 2
fi

# Sanity: the host-mock directory must exist.
if [[ ! -d "${HOST_MOCK_DIR}" ]]; then
    err "host-mock port directory not found: ${HOST_MOCK_DIR}"
    err "did you run from the repo root?"
    exit 2
fi

# --------------------------------------------------------------------------- #
# Locate gcc.                                                                 #
# --------------------------------------------------------------------------- #
if ! command -v gcc >/dev/null 2>&1; then
    err "gcc not found on PATH — install build-essential (or equivalent)."
    exit 1
fi
GCC_VERSION="$(gcc -dumpversion 2>/dev/null | head -1 || echo unknown)"
info "gcc: $(gcc --version | head -1) (dumpversion: ${GCC_VERSION})"

# --------------------------------------------------------------------------- #
# Try CMake path first (if a build/ dir exists and --no-cmake wasn't passed). #
# --------------------------------------------------------------------------- #
CMAKE_BUILD_DIR="${AFROS_HAL_BUILD_DIR:-${REPO_ROOT}/build}"

try_cmake_build() {
    if [[ "${FORCE_NO_CMAKE}" -eq 1 ]]; then
        return 1
    fi
    if ! command -v cmake >/dev/null 2>&1; then
        info "cmake not on PATH — falling back to direct gcc."
        return 1
    fi
    if [[ ! -d "${CMAKE_BUILD_DIR}" ]]; then
        info "no CMake build dir at ${CMAKE_BUILD_DIR} — falling back to direct gcc."
        return 1
    fi
    # Try to build the hal_test_runner target via CMake.
    info "trying CMake build (build dir: ${CMAKE_BUILD_DIR})"
    if cmake --build "${CMAKE_BUILD_DIR}" --target hal_test_runner \
             -- -j"$(nproc 2>/dev/null || echo 2)" 2>&1 | tee /tmp/afros-hal-cmake-build.log; then
        # Find the built binary.
        RUNNER_BIN="$(find "${CMAKE_BUILD_DIR}" -name hal_test_runner -type f -perm -u+x | head -1 || true)"
        if [[ -n "${RUNNER_BIN}" && -x "${RUNNER_BIN}" ]]; then
            ok "CMake build OK, runner at: ${RUNNER_BIN}"
            return 0
        fi
        warn "CMake build reported success but couldn't locate the hal_test_runner binary."
        warn "falling back to direct gcc."
        return 1
    else
        warn "CMake build failed — falling back to direct gcc."
        return 1
    fi
}

# --------------------------------------------------------------------------- #
# Direct gcc invocation (the CI-friendly fallback).                           #
# --------------------------------------------------------------------------- #
direct_gcc_build() {
    section "Direct gcc build (host-mock)"
    local out="/tmp/afros-hal-test-runner-${USER:-nobody}-$$"
    out="${out}.bin"

    # Every HAL src file that's part of afros-hal (see hal/CMakeLists.txt).
    local -a hal_srcs=(
        "${HAL_SRC}/hal_init.c"
        "${HAL_SRC}/device_manager.c"
        "${HAL_SRC}/network_manager.c"
        "${HAL_SRC}/power_manager.c"
        "${HAL_SRC}/gpu_manager.c"
        "${HAL_SRC}/io_subsystem.c"
        "${HAL_SRC}/virtualization.c"
        "${HAL_SRC}/kprintf.c"
    )
    # Every host-mock port src (matches ports/port-host-mock/CMakeLists.txt).
    local -a port_srcs=(
        "${HOST_MOCK_SRC}/console_port.c"
        "${HOST_MOCK_SRC}/cpu_port.c"
        "${HOST_MOCK_SRC}/memory_port.c"
        "${HOST_MOCK_SRC}/interrupt_port.c"
        "${HOST_MOCK_SRC}/timer_port.c"
        "${HOST_MOCK_SRC}/storage_port.c"
    )

    # Build the gcc command.
    #
    # NB: we deliberately do NOT pass -DAFROS_FREESTANDING=0 here — the
    # hal_test_runner.c guard is `#ifndef AFROS_FREESTANDING` (symbol
    # presence, not value), so defining it to ANY value (including 0)
    # would activate the empty-translation-unit #else branch and the
    # linker would complain "undefined reference to main".  Just leaving
    # the symbol undefined gives us the hosted runtime path we want.
    # Same caveat for hal/src/kprintf.c — its `#ifndef AFROS_FREESTANDING`
    # branch delegates to libc vprintf, which is what we want on host.
    local -a cmd=(
        gcc
        -no-pie -fno-pie
        -o "${out}"
        -I"${HAL_INCLUDE}"
        -I"${HOST_MOCK_INCLUDE}"
        -DAFROS_HOST_MOCK=1
        -Wall -Wextra -Wno-unused-parameter
        "${TESTS_DIR}/hal_test_runner.c"
        "${hal_srcs[@]}"
        "${port_srcs[@]}"
        -lm -lpthread -lrt
    )
    # Append user-supplied extra flags.
    for f in "${EXTRA_CFLAGS[@]:-}"; do [[ -n "$f" ]] && cmd+=("$f"); done
    for f in "${EXTRA_LDFLAGS[@]:-}"; do [[ -n "$f" ]] && cmd+=("$f"); done

    if [[ "${VERBOSE}" -eq 1 ]]; then
        info "gcc command:"
        printf '  %q\n' "${cmd[@]}"
    else
        info "building with gcc ($(nproc 2>/dev/null || echo 2) jobs implied, single gcc invocation)"
    fi

    if ! "${cmd[@]}"; then
        err "gcc build failed."
        return 1
    fi
    ok "build OK, runner at: ${out}"
    RUNNER_BIN="${out}"
    return 0
}

# --------------------------------------------------------------------------- #
# Run the test runner and capture output.                                     #
# --------------------------------------------------------------------------- #
run_tests() {
    section "Running HAL test runner (host-mock)"
    mkdir -p "${RESULTS_DIR}"

    local ts
    ts="$(date +%Y%m%d-%H%M%S)"
    local log="${RESULTS_DIR}/hal-tests-${ts}.log"

    info "log file: ${log}"
    info "runner:   ${RUNNER_BIN}"

    # Run with stdin redirected from /dev/null so the non-blocking getc
    # probe returns TIMEOUT immediately (no test interaction).
    # Set a 30s wall-clock cap so a runaway timer callback can't hang CI.
    set +e
    timeout --preserve-status 30 "${RUNNER_BIN}" < /dev/null > "${log}" 2>&1
    local rc=$?
    set -e

    # Print the output (verbose: full, otherwise tail).
    if [[ "${VERBOSE}" -eq 1 ]]; then
        cat "${log}"
    else
        # Tail-20 keeps the summary visible without flooding CI logs.
        tail -n 30 "${log}"
    fi

    # Surface the result line.
    echo
    if [[ ${rc} -eq 0 ]]; then
        ok "HAL test runner: PASS (exit 0, log: ${log})"
    elif [[ ${rc} -eq 124 ]]; then
        err "HAL test runner: TIMEOUT (30s, log: ${log})"
    else
        err "HAL test runner: FAIL (exit ${rc}, log: ${log})"
    fi
    # Symlink a stable name for the most-recent run.
    ln -sf "$(basename "${log}")" "${RESULTS_DIR}/hal-tests-latest.log"
    return ${rc}
}

# --------------------------------------------------------------------------- #
# Main.                                                                       #
# --------------------------------------------------------------------------- #
main() {
    section "AfriOS HAL tests (port: ${PORT})"
    local rc=0

    RUNNER_BIN=""
    if ! try_cmake_build; then
        if ! direct_gcc_build; then
            rc=1
        fi
    fi

    if [[ ${rc} -eq 0 ]]; then
        if ! run_tests; then
            rc=1
        fi
    fi

    section "Summary"
    if [[ ${rc} -eq 0 ]]; then
        ok "all HAL tests passed."
    else
        err "HAL tests failed."
    fi
    return ${rc}
}

main "$@"
