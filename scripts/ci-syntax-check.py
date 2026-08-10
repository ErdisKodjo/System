#!/usr/bin/env python3
"""
AfriOS CI syntax checker.

Walks the repository (or a sub-tree passed on the command line) and runs the
appropriate `-fsyntax-only` compiler on every source file found:

    .c / .h        -> gcc   -fsyntax-only -Wall          -x c
    .cpp / .cc     -> g++   -fsyntax-only -Wall -std=c++17 -x c++
    .m             -> gcc   -fsyntax-only -Wall          -x objective-c
    .S             -> gcc   -fsyntax-only -Wall          -x assembler-with-cpp

Checks are dispatched in parallel via `concurrent.futures.ThreadPoolExecutor`
so the wall-clock time is roughly (slowest single file) instead of
(sum of all files).  Failures are collected and printed as a summary at the
end; the script exits with status 1 if *any* file failed, 0 otherwise.

Files that the host toolchain cannot check (e.g. `.m` files when `cc1obj`
is not installed) are reported as **skipped** (warning, not failure) so a
stock `ubuntu-latest` runner can still pass the gate.

Usage:
    scripts/ci-syntax-check.py [ROOT] [--workers N] [--exclude PATH]
                                      [--extra-include PATH] [--quiet]

Exit codes:
    0  — every checked file passed syntax check
    1  — at least one file failed
    2  — invocation error (bad arguments, compiler missing)
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional


# --------------------------------------------------------------------------- #
# Configuration                                                               #
# --------------------------------------------------------------------------- #

# File extensions → (compiler, language, extra flags).
# `compiler` is resolved through PATH (so `gcc`/`g++`/`clang`/`clang++` all work
# if the runner has them installed).
EXT_RULES = {
    ".c":   ("gcc", "c",                  ["-Wall", "-Wextra"]),
    ".h":   ("gcc", "c",                  ["-Wall", "-Wextra"]),
    ".cpp": ("g++", "c++",                ["-Wall", "-Wextra", "-std=c++17"]),
    ".cc":  ("g++", "c++",                ["-Wall", "-Wextra", "-std=c++17"]),
    ".m":   ("gcc", "objective-c",        ["-Wall", "-Wextra"]),
    ".S":   ("gcc", "assembler-with-cpp", ["-Wall"]),
}

# Directories that never contain project sources we want to syntax-check.
# (Anything vendored / generated / build-artifact / VCS.)
EXCLUDE_DIRS = {
    ".git", ".hg", ".svn",
    "node_modules",
    "build", "_build",
    "CMakeFiles", "CMakeCache",
    ".cache", "__pycache__", ".pytest_cache",
    "build_test",                 # afros-dxvk/build_test/ scratch dir
    "edk2",                       # upstream EDK2 sources (huge, vendored)
    "prebuilts",                  # prebuilt binaries
    ".venv", "venv",
}

# Individual files that should be skipped (relative to repo root). These are
# typically stub markers (R.md / .md) that some module directories ship as
# placeholders.  We never syntax-check them.
EXCLUDE_FILENAMES = {
    "R.md", ".md", ".json", ".yaml", ".yml", ".cmake", ".inf", ".dec",
    ".dsc", ".vfr", ".uni", ".asl", ".nsh", ".ld", ".kconf", ".bp",
    ".ps1", ".sh", ".conf",
}

# Default number of parallel workers. Overridable via --workers or env var
# CI_SYNTAX_WORKERS. Falls back to os.cpu_count() (capped at 16 so we don't
# hammer a 64-core runner into a fork bomb of gcc processes).
DEFAULT_WORKERS = min(16, os.cpu_count() or 4)


# --------------------------------------------------------------------------- #
# Result types                                                                #
# --------------------------------------------------------------------------- #

@dataclass
class FileResult:
    """Outcome of a single file's syntax check."""
    path: Path
    kind: str                              # 'c' | 'c++' | 'objective-c' | 'assembler-with-cpp' | 'skipped'
    ok: bool                               # True if syntax check succeeded
    skipped: bool = False                  # True if we deliberately didn't check
    returncode: int = 0
    output: str = ""                       # stderr (and stdout) of the compiler
    duration_ms: int = 0


@dataclass
class Summary:
    """Aggregate result of the whole run."""
    total: int = 0
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    failures: list = field(default_factory=list)        # list[FileResult]
    skips: list = field(default_factory=list)           # list[FileResult]
    duration_ms: int = 0


# --------------------------------------------------------------------------- #
# Source discovery                                                            #
# --------------------------------------------------------------------------- #

def walk_sources(root: Path,
                 excludes: set[str],
                 extra_excludes: Iterable[str] = ()) -> Iterable[Path]:
    """Yield every source file under `root` that matches EXT_RULES.

    Prunes EXCLUDE_DIRS (and any user-supplied extra excludes) early so we
    don't waste time descending into vendored trees like edk2/.
    """
    extra_set = {os.path.normpath(e) for e in extra_excludes}
    for dirpath, dirnames, filenames in os.walk(root):
        # Mutate dirnames in place so os.walk does not descend into pruned dirs
        dirnames[:] = [d for d in dirnames
                       if d not in excludes
                       and os.path.normpath(os.path.join(dirpath, d)) not in extra_set]
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext in EXT_RULES:
                yield Path(dirpath) / fn


def collect_include_paths(root: Path) -> list[str]:
    """Walk the tree once and collect every `include/` directory.

    Passing them all as `-I` lets the syntax checker resolve any project
    header from any translation unit — looser than per-module CMake config
    but exactly what we want for a fast CI gate.
    """
    includes = []
    for dirpath, dirnames, _ in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        if os.path.basename(dirpath) == "include":
            includes.append(dirpath)
    return includes


# --------------------------------------------------------------------------- #
# Compiler probing                                                            #
# --------------------------------------------------------------------------- #

def compiler_available(name: str) -> bool:
    """True if `name` is on PATH."""
    return shutil.which(name) is not None


def probe_objective_c() -> bool:
    """Probe whether the host gcc can actually compile Objective-C.

    Returns False on a stock Debian/Ubuntu `gcc` (no `gobjc` package), so
    `.m` files can be reported as skipped rather than failing the gate.
    """
    if not compiler_available("gcc"):
        return False
    try:
        # `-x objective-c /dev/null` is the cheapest possible probe: gcc
        # either accepts it (cc1obj present) or fails with "error trying to
        # exec 'cc1obj'".
        proc = subprocess.run(
            ["gcc", "-fsyntax-only", "-x", "objective-c", "/dev/null"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            timeout=10,
        )
        return proc.returncode == 0
    except (subprocess.TimeoutExpired, OSError):
        return False


# --------------------------------------------------------------------------- #
# The per-file check                                                          #
# --------------------------------------------------------------------------- #

# Markers that, if present in the first N KB of a .h file, indicate it's
# really a C++ header that should be parsed as C++ (e.g. afros-dxvk's
# include/*.h files all #include <cstdint> and use class templates).
CPP_HEADER_MARKERS = (
    "#include <cstdint>",
    "#include <cstddef>",
    "#include <vector>",
    "#include <string>",
    "#include <memory>",
    "#include <atomic>",
    "#include <mutex>",
    "#include <thread>",
    "#include <functional>",
    "#include <optional>",
    "#include <variant>",
    "#include <map>",
    "#include <set>",
    "#include <unordered_map>",
    "#include <array>",
    "#include <span>",
    "#include <string_view>",
    "#include <format>",
    "#include <compare>",
    "extern \"C\"",
    "namespace ",
    "template <",
    "template<",
    "class ",
    "typename ",
    "std::",
)

# Markers that indicate a .h file is actually an Objective-C header (the
# afros-incompat-engine framework umbrella headers use #import / @class /
# @interface).  These are dispatched to `gcc -x objective-c` instead of C
# — when the host has gobjc installed (the CI workflow does install it).
OBJC_HEADER_MARKERS = (
    "#import ",
    "@interface ",
    "@implementation ",
    "@class ",
    "@property ",
    "@protocol ",
    "@end",
    "@selector(",
)
_CPP_PROBE_BYTES = 16 * 1024   # only scan the first 16 KiB


def _probe_header_kind(path: Path) -> str:
    """Return 'c', 'c++', or 'objective-c' for a .h file based on content.

    Heuristic, scanned against the first 16 KiB only.  Default is 'c'
    (the most common case for AfriOS headers — they use plain C with
    `extern "C"` guards).
    """
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            head = fh.read(_CPP_PROBE_BYTES)
    except OSError:
        return "c"
    # Objective-C takes precedence — an ObjC header may include <cstdint>
    # through a framework bridge, but `#import` / `@class` are unambiguous.
    if any(m in head for m in OBJC_HEADER_MARKERS):
        return "objective-c"
    if any(m in head for m in CPP_HEADER_MARKERS):
        return "c++"
    return "c"


def looks_like_cpp_header(path: Path) -> bool:
    """Backwards-compat shim — see `_probe_header_kind`."""
    return _probe_header_kind(path) == "c++"


def _resolve_rule(path: Path, rule: tuple) -> tuple:
    """Resolve the (compiler, lang, extra) triple for a file.

    For .h files we override the default C rule based on content probing:
    C++ headers (afros-dxvk/include/*.h, src/*/types.h) go to g++ -x c++,
    Objective-C umbrella headers (afros-incompat-engine/frameworks/*.h)
    go to gcc -x objective-c.
    """
    compiler, lang, extra = rule
    if path.suffix.lower() == ".h":
        kind = _probe_header_kind(path)
        if kind == "c++":
            return ("g++", "c++", ["-Wall", "-Wextra", "-std=c++17"])
        if kind == "objective-c":
            return ("gcc", "objective-c", ["-Wall", "-Wextra"])
    return rule


def build_command(path: Path, rule: tuple, include_paths: list[str]) -> list[str]:
    """Build the gcc/g++ invocation for one file."""
    compiler, lang, extra = _resolve_rule(path, rule)
    cmd = [compiler, "-fsyntax-only"]
    cmd.extend(extra)
    cmd.extend(f"-I{p}" for p in include_paths)
    # `-x` is set explicitly so we can force the language regardless of the
    # file extension (the same .h file may be C or C++ depending on its
    # content — see _resolve_rule / looks_like_cpp_header).
    cmd += ["-x", lang]
    cmd.append(str(path))
    return cmd


def check_one(path: Path, include_paths: list[str],
              objc_enabled: bool) -> FileResult:
    """Run the syntax checker on one file and return a FileResult."""
    ext = path.suffix.lower()
    rule = EXT_RULES[ext]
    # Resolve to the actual (compiler, lang, extra) we'll use — this lets
    # the FileResult.kind reflect whether a .h file was checked as C or C++.
    compiler, lang, _ = _resolve_rule(path, rule)

    # Skip Objective-C if the host has no cc1obj.  We surface this as a
    # "skipped" result so the CI gate stays green on stock runners but the
    # user is told they're missing coverage.
    if lang == "objective-c" and not objc_enabled:
        return FileResult(path=path, kind=lang, ok=False, skipped=True,
                          output="cc1obj not available on this host (install gobjc or clang)")

    if not compiler_available(compiler):
        return FileResult(path=path, kind=lang, ok=False, skipped=True,
                          output=f"compiler '{compiler}' not on PATH")

    cmd = build_command(path, rule, include_paths)
    t0 = time.monotonic()
    try:
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,   # 2 min hard cap per file
        )
        dt = int((time.monotonic() - t0) * 1000)
        ok = proc.returncode == 0
        return FileResult(
            path=path, kind=lang, ok=ok, skipped=False,
            returncode=proc.returncode,
            output=proc.stdout.decode("utf-8", errors="replace"),
            duration_ms=dt,
        )
    except subprocess.TimeoutExpired:
        dt = int((time.monotonic() - t0) * 1000)
        return FileResult(path=path, kind=lang, ok=False, skipped=False,
                          returncode=124,
                          output="TIMEOUT after 120s",
                          duration_ms=dt)
    except OSError as exc:
        dt = int((time.monotonic() - t0) * 1000)
        return FileResult(path=path, kind=lang, ok=False, skipped=False,
                          returncode=125, output=str(exc), duration_ms=dt)


# --------------------------------------------------------------------------- #
# Output formatting                                                           #
# --------------------------------------------------------------------------- #

def _truncate(s: str, n: int = 400) -> str:
    s = s.strip()
    return s if len(s) <= n else s[:n] + " … [truncated]"


def print_summary(summary: Summary, verbose: bool) -> None:
    """Print human-readable summary to stderr (so it appears in CI logs)."""
    print("", file=sys.stderr)
    print("=" * 78, file=sys.stderr)
    print(f" AfriOS syntax-check summary", file=sys.stderr)
    print("=" * 78, file=sys.stderr)
    print(f"  Total files : {summary.total}", file=sys.stderr)
    print(f"  Passed      : {summary.passed}", file=sys.stderr)
    print(f"  Failed      : {summary.failed}", file=sys.stderr)
    print(f"  Skipped     : {summary.skipped}", file=sys.stderr)
    print(f"  Wall time   : {summary.duration_ms / 1000:.1f} s", file=sys.stderr)
    print("-" * 78, file=sys.stderr)

    if summary.failures:
        print(f"\n FAILURES ({len(summary.failures)}):", file=sys.stderr)
        for r in summary.failures:
            rel = r.path
            print(f"  ✗ {rel}", file=sys.stderr)
            if verbose or True:
                # Always print the (truncated) compiler output for failures;
                # without it, the gate is useless for debugging.
                for line in _truncate(r.output).splitlines():
                    print(f"      {line}", file=sys.stderr)

    if summary.skips:
        print(f"\n SKIPPED ({len(summary.skips)}):", file=sys.stderr)
        for r in summary.skips:
            print(f"  ⊘ {r.path}  ({_truncate(r.output, 80)})", file=sys.stderr)

    print("=" * 78, file=sys.stderr)


# --------------------------------------------------------------------------- #
# Entry point                                                                 #
# --------------------------------------------------------------------------- #

def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="ci-syntax-check.py",
        description="Parallel syntax checker for AfriOS source files.",
    )
    p.add_argument("root", nargs="?", default=".",
                   help="Repository root to scan (default: current dir)")
    p.add_argument("--workers", type=int,
                   default=int(os.environ.get("CI_SYNTAX_WORKERS", DEFAULT_WORKERS)),
                   help=f"Parallel workers (default: {DEFAULT_WORKERS})")
    p.add_argument("--exclude", action="append", default=[],
                   help="Additional path to exclude (repeatable)")
    p.add_argument("--extra-include", action="append", default=[],
                   help="Additional -I path (repeatable)")
    p.add_argument("--quiet", action="store_true",
                   help="Print only failures and the final summary")
    p.add_argument("--verbose", action="store_true",
                   help="Print every file as it is checked")
    return p.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])

    root = Path(args.root).resolve()
    if not root.is_dir():
        print(f"error: '{root}' is not a directory", file=sys.stderr)
        return 2

    # Pre-flight: do we have at least one compiler?
    if not (compiler_available("gcc") or compiler_available("clang")):
        print("error: neither gcc nor clang is on PATH", file=sys.stderr)
        return 2

    objc_enabled = probe_objective_c()
    if not objc_enabled:
        print("warning: cc1obj not available — .m files will be skipped",
              file=sys.stderr)

    # Collect include paths once (cheap, single walk).
    include_paths = collect_include_paths(root) + list(args.extra_include)
    if not args.quiet:
        print(f"Using {len(include_paths)} -I include paths", file=sys.stderr)

    # Enumerate sources.
    sources = list(walk_sources(root, EXCLUDE_DIRS, args.exclude))
    if not sources:
        print("warning: no source files found", file=sys.stderr)
        return 0

    if not args.quiet:
        print(f"Checking {len(sources)} files with {args.workers} workers…",
              file=sys.stderr)

    summary = Summary(total=len(sources))
    t0 = time.monotonic()

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        future_to_path = {
            pool.submit(check_one, p, include_paths, objc_enabled): p
            for p in sources
        }
        for i, fut in enumerate(as_completed(future_to_path), 1):
            result = fut.result()
            if result.skipped:
                summary.skipped += 1
                summary.skips.append(result)
            elif result.ok:
                summary.passed += 1
                if args.verbose and not args.quiet:
                    print(f"  ✓ {result.path}", file=sys.stderr)
            else:
                summary.failed += 1
                summary.failures.append(result)
            if not args.quiet and i % 25 == 0:
                pct = 100.0 * i / summary.total
                print(f"  … {i}/{summary.total} ({pct:.0f}%) "
                      f"passed={summary.passed} failed={summary.failed} "
                      f"skipped={summary.skipped}", file=sys.stderr)

    summary.duration_ms = int((time.monotonic() - t0) * 1000)
    print_summary(summary, args.verbose)

    # Also emit a GitHub Actions summary table if running under GHA.
    if os.environ.get("GITHUB_ACTIONS") == "true":
        try:
            step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
            if step_summary:
                with open(step_summary, "a", encoding="utf-8") as fh:
                    fh.write("\n### Syntax-check summary\n\n")
                    fh.write("| Metric | Count |\n|---|---|\n")
                    fh.write(f"| Total files | {summary.total} |\n")
                    fh.write(f"| Passed | {summary.passed} |\n")
                    fh.write(f"| Failed | {summary.failed} |\n")
                    fh.write(f"| Skipped | {summary.skipped} |\n")
                    fh.write(f"| Wall time | {summary.duration_ms / 1000:.1f}s |\n")
                    if summary.failures:
                        fh.write("\n#### Failures\n\n")
                        fh.write("| File | Error |\n|---|---|\n")
                        for r in summary.failures[:50]:
                            rel = r.path.relative_to(root) if r.path.is_relative_to(root) else r.path
                            err = _truncate(r.output, 200).replace("|", "\\|").replace("\n", " ")
                            fh.write(f"| `{rel}` | {err} |\n")
                        if len(summary.failures) > 50:
                            fh.write(f"\n… and {len(summary.failures) - 50} more.\n")
        except OSError:
            pass  # Don't fail the job because we couldn't write the summary.

    return 1 if summary.failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
