#!/usr/bin/env python3
"""
AfriOS CI artifacts summary generator.

Walks one or more `build/` directories (and any other paths passed on the
command line) looking for built artifacts, computes a SHA-256 + size for
each, and emits:

    artifacts-summary.md      — human-readable Markdown table
    artifacts-summary.json    — machine-readable manifest

The Markdown table is suitable for `$GITHUB_STEP_SUMMARY` inclusion so the
list of artifacts produced by a CI run shows up directly on the run's
summary page.

Recognized artifact extensions:
    .elf   — kernel images (per-arch)
    .so    — shared libraries (compat layers, runtime managers)
    .a     — static archives (afros-hal, afros-corebridge, etc.)
    .fd    — UEFI firmware images (EDK2 build outputs)
    .bin   — raw binary payloads (e.g. capsule images, MCU .bin)
    .dll.so — Wine-loadable DXVK aliases (symlinks are followed)
    .ko    — kernel modules (if/when built)

Usage:
    scripts/ci-artifacts-summary.py [PATH...] [--root REPO_ROOT]
                                    [--md-out FILE] [--json-out FILE]
                                    [--label TEXT]

Defaults:
    PATH       = build/ (relative to CWD)
    --md-out   = artifacts-summary.md
    --json-out = artifacts-summary.json
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Iterable, Optional


# --------------------------------------------------------------------------- #
# Configuration                                                               #
# --------------------------------------------------------------------------- #

ARTIFACT_EXTENSIONS = {
    ".elf":    "kernel-image",
    ".so":     "shared-library",
    ".a":      "static-archive",
    ".fd":     "uefi-firmware",
    ".bin":    "raw-binary",
    ".dll.so": "wine-alias",
    ".ko":     "kernel-module",
    ".dylib":  "shared-library",
}

# Multi-part extensions need special handling: a file like `d3d9.dll.so`
# has stem `d3d9` and suffix `.so` if you ask pathlib, but we want to
# recognise the `.dll.so` suffix.
MULTI_PART_SUFFIXES = (".dll.so", ".so.1", ".so.0")

# Prune these subdirectories even when they sit inside a build tree.
EXCLUDE_DIRS = {
    "CMakeFiles", ".git", "__pycache__", ".cache",
    "Testing", "tmp",
}

# Skip individual files larger than this (256 MiB) — they're almost
# certainly not real artifacts and computing their sha256 would just
# waste CI time. Override via --max-size-mb.
DEFAULT_MAX_SIZE_MB = 256


# --------------------------------------------------------------------------- #
# Data types                                                                  #
# --------------------------------------------------------------------------- #

@dataclass
class Artifact:
    """One built artifact entry."""
    path: str               # repo-relative path (POSIX form)
    absolute_path: str      # absolute path on disk
    kind: str               # from ARTIFACT_EXTENSIONS
    size_bytes: int
    sha256: str             # 64-char lowercase hex digest
    mtime: str              # ISO 8601 UTC

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class Manifest:
    """Top-level JSON manifest."""
    schema_version: int = 1
    generated_at: str = ""
    repo_root: str = ""
    total_artifacts: int = 0
    total_size_bytes: int = 0
    artifacts: list = None  # type: ignore[assignment]

    def __post_init__(self):
        if self.artifacts is None:
            self.artifacts = []


# --------------------------------------------------------------------------- #
# Helpers                                                                     #
# --------------------------------------------------------------------------- #

def _suffix_of(path: Path) -> str:
    """Return the matching artifact suffix (handles multi-part suffixes).

    For `d3d9.dll.so` returns `.dll.so`; for `libfoo.so` returns `.so`;
    for `baz.elf` returns `.elf`.
    """
    name = path.name
    for mp in MULTI_PART_SUFFIXES:
        if name.endswith(mp):
            return mp
    return path.suffix


def _kind_for(path: Path) -> Optional[str]:
    """Return the artifact kind for `path`, or None if not an artifact."""
    suffix = _suffix_of(path)
    return ARTIFACT_EXTENSIONS.get(suffix)


def _sha256(path: Path, chunk: int = 1 << 20) -> str:
    """Stream a file through sha256, returning the hex digest."""
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        while True:
            buf = fh.read(chunk)
            if not buf:
                break
            h.update(buf)
    return h.hexdigest()


def _iso_utc(epoch: float) -> str:
    return _dt.datetime.fromtimestamp(epoch, tz=_dt.timezone.utc).isoformat()


def _human_size(n: int) -> str:
    """Human-readable file size (e.g. `1.4 MiB`)."""
    if n < 1024:
        return f"{n} B"
    for unit in ("KiB", "MiB", "GiB", "TiB"):
        n /= 1024.0
        if n < 1024:
            return f"{n:.1f} {unit}"
    return f"{n:.1f} PiB"


# --------------------------------------------------------------------------- #
# Discovery                                                                   #
# --------------------------------------------------------------------------- #

def walk_artifacts(roots: Iterable[Path],
                   exclude_dirs: set[str],
                   max_size_bytes: int) -> Iterable[Artifact]:
    """Yield Artifact objects for every recognized file under `roots`."""
    seen: set[Path] = set()
    for root in roots:
        if not root.exists():
            continue
        # Allow the caller to pass a single file as a root.
        if root.is_file():
            kind = _kind_for(root)
            if kind and root not in seen:
                seen.add(root)
                a = _build_artifact(root, kind, max_size_bytes)
                if a:
                    yield a
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in exclude_dirs]
            for fn in filenames:
                p = Path(dirpath) / fn
                kind = _kind_for(p)
                if not kind:
                    continue
                # Resolve symlinks so each unique target is recorded once
                # (this matters for the d3d9.dll.so → libafros-dxvk.so links).
                try:
                    real = p.resolve(strict=False)
                except OSError:
                    real = p
                if real in seen:
                    continue
                seen.add(real)
                a = _build_artifact(p, kind, max_size_bytes)
                if a:
                    yield a


def _build_artifact(path: Path, kind: str, max_size_bytes: int) -> Optional[Artifact]:
    """Construct a single Artifact (or None if it should be skipped)."""
    try:
        st = path.stat()
    except OSError:
        return None
    if st.st_size > max_size_bytes:
        return None
    try:
        digest = _sha256(path)
    except OSError:
        return None
    return Artifact(
        path=str(path),
        absolute_path=str(path.resolve(strict=False)),
        kind=kind,
        size_bytes=st.st_size,
        sha256=digest,
        mtime=_iso_utc(st.st_mtime),
    )


# --------------------------------------------------------------------------- #
# Output                                                                      #
# --------------------------------------------------------------------------- #

def write_markdown(manifest: Manifest, out: Path, repo_root: Optional[Path] = None,
                   label: str = "Artifacts") -> None:
    """Write the Markdown summary table to `out`."""
    lines: list[str] = []
    lines.append(f"## {label}")
    lines.append("")
    lines.append(f"_Generated {manifest.generated_at}_")
    lines.append("")
    if not manifest.artifacts:
        lines.append("_No artifacts found._")
        lines.append("")
        out.write_text("\n".join(lines), encoding="utf-8")
        return

    # Group by kind so the table is skimmable.
    by_kind: dict[str, list[Artifact]] = {}
    for a in manifest.artifacts:
        by_kind.setdefault(a.kind, []).append(a)

    for kind in sorted(by_kind):
        entries = sorted(by_kind[kind], key=lambda a: a.path)
        lines.append(f"### {kind} ({len(entries)})")
        lines.append("")
        lines.append("| Path | Size | SHA-256 (first 16) |")
        lines.append("|---|---:|---|")
        for a in entries:
            rel = _relative_or_path(a.path, repo_root)
            short = a.sha256[:16]
            lines.append(f"| `{rel}` | {_human_size(a.size_bytes)} | `{short}…` |")
        lines.append("")

    lines.append("### Totals")
    lines.append("")
    lines.append(f"- **Artifacts**: {manifest.total_artifacts}")
    lines.append(f"- **Total size**: {_human_size(manifest.total_size_bytes)}")
    lines.append("")
    out.write_text("\n".join(lines), encoding="utf-8")


def write_json(manifest: Manifest, out: Path) -> None:
    """Write the JSON manifest to `out` (pretty-printed)."""
    payload = {
        "schema_version": manifest.schema_version,
        "generated_at": manifest.generated_at,
        "repo_root": manifest.repo_root,
        "total_artifacts": manifest.total_artifacts,
        "total_size_bytes": manifest.total_size_bytes,
        "artifacts": [a.to_dict() for a in manifest.artifacts],
    }
    out.write_text(json.dumps(payload, indent=2, sort_keys=False) + "\n",
                   encoding="utf-8")


def _relative_or_path(p: str, root: Optional[Path]) -> str:
    if root is None:
        return p
    try:
        return str(Path(p).relative_to(root))
    except ValueError:
        return p


# --------------------------------------------------------------------------- #
# Entry point                                                                 #
# --------------------------------------------------------------------------- #

def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="ci-artifacts-summary.py",
        description="Walk build/ directories and emit a Markdown + JSON "
                    "artifact manifest with SHA-256 checksums.",
    )
    p.add_argument("paths", nargs="*", default=["build"],
                   help="Directories (or files) to scan (default: build/)")
    p.add_argument("--root", default=".",
                   help="Repository root, used to make paths relative "
                        "in the Markdown table (default: .)")
    p.add_argument("--md-out", default="artifacts-summary.md",
                   help="Markdown output file (default: artifacts-summary.md)")
    p.add_argument("--json-out", default="artifacts-summary.json",
                   help="JSON output file (default: artifacts-summary.json)")
    p.add_argument("--label", default="AfriOS build artifacts",
                   help="Markdown section heading (default: 'AfriOS build artifacts')")
    p.add_argument("--max-size-mb", type=int, default=DEFAULT_MAX_SIZE_MB,
                   help=f"Skip files larger than this (default: {DEFAULT_MAX_SIZE_MB} MiB)")
    p.add_argument("--quiet", action="store_true",
                   help="Suppress per-file stdout logging")
    return p.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])

    repo_root = Path(args.root).resolve()
    roots = [Path(p) for p in args.paths]
    max_size = args.max_size_mb * (1 << 20)

    if not args.quiet:
        print(f"Scanning {len(roots)} path(s) for build artifacts…",
              file=sys.stderr)

    manifest = Manifest(
        generated_at=_iso_utc(_dt.datetime.now().timestamp()),
        repo_root=str(repo_root),
    )

    for art in walk_artifacts(roots, EXCLUDE_DIRS, max_size):
        manifest.artifacts.append(art)
        manifest.total_size_bytes += art.size_bytes
        if not args.quiet:
            print(f"  + {art.kind:18s} {_human_size(art.size_bytes):>10s}  "
                  f"{art.sha256[:12]}  {art.path}", file=sys.stderr)

    manifest.total_artifacts = len(manifest.artifacts)

    # Write outputs.
    md_out = Path(args.md_out)
    json_out = Path(args.json_out)
    write_markdown(manifest, md_out, repo_root=repo_root, label=args.label)
    write_json(manifest, json_out)

    if not args.quiet:
        print("", file=sys.stderr)
        print(f"Wrote {md_out} and {json_out}", file=sys.stderr)
        print(f"  {manifest.total_artifacts} artifacts, "
              f"{_human_size(manifest.total_size_bytes)} total", file=sys.stderr)

    # Also append to GITHUB_STEP_SUMMARY if running under GHA.
    if os.environ.get("GITHUB_ACTIONS") == "true":
        step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
        if step_summary:
            try:
                with open(step_summary, "a", encoding="utf-8") as fh:
                    fh.write(md_out.read_text(encoding="utf-8"))
                    fh.write("\n")
            except OSError:
                pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
