#!/usr/bin/env python3
# =============================================================================
# compat-test-harness.py — Framework de test de compatibilité AfriOS
#
# Discrétise tests/<platform>/<test-name>/test.json, exécute le binaire
# via le point d'entrée utilisateur `afros-launch <binary>`, capture
# stdout/stderr/exit_code/duration, calcule un score 0-100 par test,
# et émet un rapport JSON + Markdown.
#
# Usage:
#   python3 tests/compat-test-harness.py                       # tout
#   python3 tests/compat-test-harness.py --platform windows    # filtre
#   python3 tests/compat-test-harness.py --test windows/hello-world
#   python3 tests/compat-test-harness.py --dry-run             # découverte
#   python3 tests/compat-test-harness.py --verbose             # logs
#   python3 tests/compat-test-harness.py --jobs 4              // parallèle
#
# Requires: Python 3.10+
# =============================================================================
"""AfriOS compatibility test harness.

Discovers per-platform test cases described by ``test.json`` manifests,
runs them through the ``afros-launch`` orchestrator entry point, scores
each on a 0-100 scale according to manifest-defined criteria, and emits
JSON + Markdown reports into ``tests/results/``.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

# --- Constants --------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
TESTS_DIR = REPO_ROOT / "tests"
RESULTS_DIR = TESTS_DIR / "results"
SUPPORTED_PLATFORMS = ("windows", "android", "ios", "harmonyos", "linux")
AFROS_LAUNCH = os.environ.get("AFROS_LAUNCH", "afros-launch")
DEFAULT_TIMEOUT_MS = 5000


# --- Data models ------------------------------------------------------------


@dataclass
class TestManifest:
    """Représentation typée d'un fichier test.json."""

    name: str
    description: str
    binary: str
    platform: str
    path: Path
    expected_output: str = ""
    timeout_ms: int = DEFAULT_TIMEOUT_MS
    scoring: dict[str, int] = field(default_factory=dict)
    extra: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_json(cls, path: Path) -> "TestManifest":
        with path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
        platform = path.parent.parent.name
        return cls(
            name=data["name"],
            description=data.get("description", ""),
            binary=data["binary"],
            platform=platform,
            path=path.parent,
            expected_output=data.get("expected_output", ""),
            timeout_ms=int(data.get("timeout_ms", DEFAULT_TIMEOUT_MS)),
            scoring=dict(data.get("scoring", {})),
            extra={k: v for k, v in data.items() if k not in {
                "name", "description", "binary", "expected_output",
                "timeout_ms", "scoring",
            }},
        )


@dataclass
class TestResult:
    """Résultat d'exécution d'un test."""

    name: str
    platform: str
    binary: str
    score: int
    stdout: str
    stderr: str
    exit_code: int
    duration_ms: int
    timed_out: bool
    skipped: bool
    skip_reason: str
    criteria: dict[str, bool] = field(default_factory=dict)

    @property
    def passed(self) -> bool:
        return (not self.skipped) and self.score == 100


# --- Abstract base ----------------------------------------------------------


class CompatTest(ABC):
    """Classe de base abstraite d'un test de compatibilité.

    Les sous-classes concrètes (une par plateforme) surchargent
    ``launch_command`` pour produire la ligne de commande réelle
    (souvent ``afros-launch <binary>`` mais parfois un wrapper
    spécifique — p. ex. ``dalvikvm`` pour Android).
    """

    def __init__(self, manifest: TestManifest) -> None:
        self.manifest = manifest

    # -- lifecycle hooks ----------------------------------------------------
    def setup(self) -> None:
        """Pré-exécution (création de fichiers, variables, …)."""

    def teardown(self) -> None:
        """Post-exécution (nettoyage)."""

    @abstractmethod
    def launch_command(self) -> list[str]:
        """Construit la commande shell utilisée pour lancer le test."""

    # -- main entry ---------------------------------------------------------
    def run(self, verbose: bool = False) -> TestResult:
        """Exécute le test, capture les sorties, calcule le score."""
        launcher = AFROS_LAUNCH.split()[0]
        if not shutil.which(launcher):
            # afros-launch absent du PATH -> skip propre plutôt que crash.
            # Le --dry-run reste fonctionnel car il n'appelle pas run().
            return TestResult(
                name=self.manifest.name,
                platform=self.manifest.platform,
                binary=self.manifest.binary,
                score=0,
                stdout="",
                stderr=f"{launcher} not found on PATH",
                exit_code=-1,
                duration_ms=0,
                timed_out=False,
                skipped=True,
                skip_reason=f"{launcher} not installed",
            )

        binary_path = self.manifest.path / self.manifest.binary
        if not binary_path.exists():
            return TestResult(
                name=self.manifest.name,
                platform=self.manifest.platform,
                binary=self.manifest.binary,
                score=0,
                stdout="",
                stderr=f"binary not found: {binary_path}",
                exit_code=-1,
                duration_ms=0,
                timed_out=False,
                skipped=True,
                skip_reason="binary missing — build it first",
            )

        cmd = self.launch_command()
        if verbose:
            print(f"  [run] {' '.join(cmd)}", file=sys.stderr)

        timeout_s = self.manifest.timeout_ms / 1000.0
        start = time.monotonic()
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout_s,
                cwd=str(self.manifest.path),
            )
            stdout = proc.stdout
            stderr = proc.stderr
            exit_code = proc.returncode
            timed_out = False
        except subprocess.TimeoutExpired as exc:
            stdout = (exc.stdout or b"").decode("utf-8", errors="replace") \
                if isinstance(exc.stdout, bytes) else (exc.stdout or "")
            stderr = (exc.stderr or b"").decode("utf-8", errors="replace") \
                if isinstance(exc.stderr, bytes) else (exc.stderr or "")
            exit_code = -1
            timed_out = True

        duration_ms = int((time.monotonic() - start) * 1000)

        score, criteria = self._score(stdout, exit_code, duration_ms, timed_out)
        return TestResult(
            name=self.manifest.name,
            platform=self.manifest.platform,
            binary=self.manifest.binary,
            score=score,
            stdout=stdout,
            stderr=stderr,
            exit_code=exit_code,
            duration_ms=duration_ms,
            timed_out=timed_out,
            skipped=False,
            skip_reason="",
            criteria=criteria,
        )

    # -- scoring ------------------------------------------------------------
    def _score(
        self,
        stdout: str,
        exit_code: int,
        duration_ms: int,
        timed_out: bool,
    ) -> tuple[int, dict[str, bool]]:
        """Calcule un score 0-100 pondéré par les critères du manifest."""
        scoring = self.manifest.scoring or {
            "stdout_match": 50,
            "exit_code_zero": 30,
            "completes_under_timeout": 20,
        }
        criteria: dict[str, bool] = {}

        # stdout_match : la sortie attendue apparaît dans stdout.
        if self.manifest.expected_output:
            criteria["stdout_match"] = (
                self.manifest.expected_output in stdout
            )
        else:
            # Pas de sortie attendue -> critère automatiquement vrai.
            criteria["stdout_match"] = True

        # exit_code_zero : le process s'est terminé normalement.
        criteria["exit_code_zero"] = (exit_code == 0)

        # completes_under_timeout : n'a pas atteint le timeout et
        # la durée mesurée reste sous la limite du manifest.
        criteria["completes_under_timeout"] = (
            not timed_out and duration_ms <= self.manifest.timeout_ms
        )

        score = 0
        for key, weight in scoring.items():
            if criteria.get(key, False):
                score += int(weight)

        # Plafonner à 100 (les poids peuvent dépasser 100 si l'auteur
        # du test a mal dosé, mais le score reste une pourcentage).
        return min(score, 100), criteria


# --- Concrete platform adapters --------------------------------------------


class GenericAfriOSTest(CompatTest):
    """Adaptateur par défaut : appelle `afros-launch <binary>`."""

    def launch_command(self) -> list[str]:
        return [AFROS_LAUNCH, self.manifest.binary]


class AndroidTest(CompatTest):
    """Android : si le binaire est un .dex, on utilise dalvikvm via
    afros-launch ; sinon afros-launch direct."""

    def launch_command(self) -> list[str]:
        if self.manifest.binary.endswith(".dex"):
            # Le runtime Android (afros-androsandbox) expose dalvikvm.
            entry = self.manifest.extra.get("entry_class",
                                            "com.afrios.Hello")
            return [AFROS_LAUNCH, "--runtime=android",
                    f"--entry={entry}", self.manifest.binary]
        return [AFROS_LAUNCH, "--runtime=android", self.manifest.binary]


class IOSTest(CompatTest):
    """iOS/macOS : afros-launch --runtime=ios pour passer par Darling."""

    def launch_command(self) -> list[str]:
        return [AFROS_LAUNCH, "--runtime=ios", self.manifest.binary]


class HarmonyOSTest(CompatTest):
    """HarmonyOS : afros-launch --runtime=harmony sur un .hap."""

    def launch_command(self) -> list[str]:
        return [AFROS_LAUNCH, "--runtime=harmony", self.manifest.binary]


class LinuxTest(CompatTest):
    """Linux natif : exécution directe, sans runtime de compatibilité."""

    def launch_command(self) -> list[str]:
        # Linux = baseline. On peut appeler afros-launch --runtime=linux
        # ou exécuter directement le binaire si afros-launch est absent.
        return [AFROS_LAUNCH, "--runtime=linux", self.manifest.binary]


PLATFORM_ADAPTERS = {
    "windows": GenericAfriOSTest,
    "android": AndroidTest,
    "ios": IOSTest,
    "harmonyos": HarmonyOSTest,
    "linux": LinuxTest,
}


# --- Discovery --------------------------------------------------------------


def discover_tests(
    platform_filter: str | None = None,
    test_filter: str | None = None,
) -> list[TestManifest]:
    """Scanne tests/<platform>/*/test.json et retourne la liste triée."""
    manifests: list[TestManifest] = []
    platforms = [platform_filter] if platform_filter else SUPPORTED_PLATFORMS
    for plat in platforms:
        plat_dir = TESTS_DIR / plat
        if not plat_dir.is_dir():
            continue
        for entry in sorted(plat_dir.iterdir()):
            if not entry.is_dir():
                continue
            manifest_path = entry / "test.json"
            if not manifest_path.is_file():
                continue
            try:
                m = TestManifest.from_json(manifest_path)
            except (json.JSONDecodeError, KeyError) as exc:
                print(f"WARN: bad manifest {manifest_path}: {exc}",
                      file=sys.stderr)
                continue
            if test_filter:
                key = f"{plat}/{m.name}"
                if key != test_filter and m.name != test_filter:
                    continue
            manifests.append(m)
    return manifests


def build_test(manifest: TestManifest) -> CompatTest:
    """Construit l'instance de test appropriée pour la plateforme."""
    adapter = PLATFORM_ADAPTERS.get(manifest.platform, GenericAfriOSTest)
    return adapter(manifest)


# --- Reporting --------------------------------------------------------------


def render_markdown_report(results: list[TestResult],
                           timestamp: str) -> str:
    """Génère le rapport Markdown final."""
    lines: list[str] = []
    lines.append("# Rapport de compatibilité AfriOS")
    lines.append("")
    lines.append(f"**Date:** {timestamp}")
    lines.append(f"**Total tests:** {len(results)}")
    passed = sum(1 for r in results if r.passed)
    skipped = sum(1 for r in results if r.skipped)
    failed = len(results) - passed - skipped
    avg = (sum(r.score for r in results) / len(results)) if results else 0
    lines.append(f"**Réussis:** {passed} | **Échoués:** {failed} | "
                 f"**Skippés:** {skipped}")
    lines.append(f"**Score moyen:** {avg:.1f}/100")
    lines.append("")
    lines.append("## Par plateforme")
    lines.append("")
    lines.append("| Plateforme | Tests | Score moyen | Réussis |")
    lines.append("|---|---|---|---|")
    for plat in SUPPORTED_PLATFORMS:
        plat_results = [r for r in results if r.platform == plat]
        if not plat_results:
            continue
        plat_avg = (sum(r.score for r in plat_results)
                    / len(plat_results))
        plat_pass = sum(1 for r in plat_results if r.passed)
        lines.append(f"| {plat} | {len(plat_results)} | "
                     f"{plat_avg:.1f} | {plat_pass} |")
    lines.append("")
    lines.append("## Détails par test")
    lines.append("")
    lines.append("| Plateforme | Test | Score | Durée (ms) | "
                 "Exit | Critères |")
    lines.append("|---|---|---|---|---|---|")
    for r in sorted(results, key=lambda x: (x.platform, x.name)):
        crit = ", ".join(
            f"{k}={'✓' if v else '✗'}" for k, v in r.criteria.items()
        ) or "—"
        status = "SKIP" if r.skipped else r.exit_code
        lines.append(f"| {r.platform} | {r.name} | {r.score} | "
                     f"{r.duration_ms} | {status} | {crit} |")
    lines.append("")
    lines.append("## Sorties brutes")
    for r in sorted(results, key=lambda x: (x.platform, x.name)):
        lines.append("")
        lines.append(f"### {r.platform}/{r.name}")
        if r.skipped:
            lines.append(f"_Skippé: {r.skip_reason}_")
            continue
        lines.append(f"- **Exit code:** {r.exit_code}")
        lines.append(f"- **Duration:** {r.duration_ms} ms")
        lines.append(f"- **Timed out:** {r.timed_out}")
        lines.append("- **stdout:**")
        lines.append("```")
        lines.append(r.stdout[:4096])
        lines.append("```")
        if r.stderr:
            lines.append("- **stderr:**")
            lines.append("```")
            lines.append(r.stderr[:4096])
            lines.append("```")
    return "\n".join(lines) + "\n"


# --- Main -------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="AfriOS compatibility test harness",
    )
    parser.add_argument("--platform", choices=SUPPORTED_PLATFORMS,
                        help="limiter à une plateforme")
    parser.add_argument("--test",
                        help="limiter à un test (plateforme/nom ou nom)")
    parser.add_argument("--verbose", action="store_true",
                        help="logs détaillés")
    parser.add_argument("--dry-run", action="store_true",
                        help="lister les tests découverts sans exécuter")
    parser.add_argument("--jobs", type=int, default=1,
                        help="exécution parallèle (N workers)")
    parser.add_argument("--results-dir", type=Path, default=RESULTS_DIR,
                        help="répertoire de sortie des rapports")
    args = parser.parse_args(argv)

    manifests = discover_tests(args.platform, args.test)
    if not manifests:
        print("Aucun test découvert.", file=sys.stderr)
        return 1

    if args.dry_run:
        print(f"Tests découverts: {len(manifests)}")
        for m in manifests:
            print(f"  {m.platform}/{m.name}  binary={m.binary}  "
                  f"timeout={m.timeout_ms}ms")
        return 0

    args.results_dir.mkdir(parents=True, exist_ok=True)
    timestamp = time.strftime("%Y%m%d-%H%M%S")

    tests = [build_test(m) for m in manifests]

    results: list[TestResult] = []
    if args.jobs <= 1:
        for t in tests:
            t.setup()
            try:
                results.append(t.run(verbose=args.verbose))
            finally:
                t.teardown()
    else:
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(_safe_run, t, args.verbose): t
                       for t in tests}
            for fut in as_completed(futures):
                results.append(fut.result())

    results.sort(key=lambda r: (r.platform, r.name))

    # JSON report
    json_path = args.results_dir / f"results-{timestamp}.json"
    with json_path.open("w", encoding="utf-8") as fh:
        json.dump([asdict(r) for r in results], fh,
                  indent=2, ensure_ascii=False)

    # Markdown report
    md_path = args.results_dir / f"report-{timestamp}.md"
    md_path.write_text(render_markdown_report(results, timestamp),
                       encoding="utf-8")

    # Console summary
    print("\n=== Résumé ===")
    for r in results:
        status = "SKIP" if r.skipped else ("PASS" if r.passed else "FAIL")
        print(f"  [{status:4}] {r.platform}/{r.name}: "
              f"{r.score}/100  ({r.duration_ms}ms)")
    print(f"\nRapports: {json_path}")
    print(f"          {md_path}")

    return 0 if all(r.passed or r.skipped for r in results) else 1


def _safe_run(test: CompatTest, verbose: bool) -> TestResult:
    """Wrapper pour exécution parallèle : setup/teardown protégés."""
    try:
        test.setup()
    except Exception as exc:  # noqa: BLE001
        return TestResult(
            name=test.manifest.name,
            platform=test.manifest.platform,
            binary=test.manifest.binary,
            score=0, stdout="", stderr=str(exc), exit_code=-1,
            duration_ms=0, timed_out=False, skipped=True,
            skip_reason=f"setup error: {exc}",
        )
    try:
        return test.run(verbose=verbose)
    finally:
        try:
            test.teardown()
        except Exception:  # noqa: BLE001
            pass


if __name__ == "__main__":
    sys.exit(main())
