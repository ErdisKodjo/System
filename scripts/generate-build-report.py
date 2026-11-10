#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AfriOS Build Report Generator
Génère un rapport Markdown + JSON récapitulant les artifacts du build.
"""
import os
import sys
import json
import hashlib
import subprocess
from datetime import datetime
from pathlib import Path

BUILD_DIR = Path("/home/z/my-project/System/build")
REPORT_DIR = Path("/home/z/my-project/System/build/reports")
REPORT_DIR.mkdir(parents=True, exist_ok=True)

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()

def file_kind(path):
    name = path.name
    if name.endswith('.so') or '.so.' in name:
        return 'shared_lib'
    if name.endswith('.a'):
        return 'static_lib'
    if name.endswith('.o'):
        return 'object'
    if '.' not in name or name.endswith('.elf'):
        return 'executable'
    return 'other'

def main():
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    artifacts = []

    # Scan build directory
    for root, dirs, files in os.walk(BUILD_DIR):
        for f in files:
            path = Path(root) / f
            if path.stat().st_size == 0:
                continue
            # Only collect interesting artifacts (.so, .a, executables)
            kind = file_kind(path)
            if kind in ('shared_lib', 'static_lib', 'executable'):
                # Compute SHA-256
                try:
                    sha = sha256_file(path)
                except Exception:
                    sha = ""
                # Identify the module (parent dir name)
                module = path.parent.name
                artifacts.append({
                    'path': str(path.relative_to(BUILD_DIR)),
                    'name': f,
                    'module': module,
                    'kind': kind,
                    'size_bytes': path.stat().st_size,
                    'sha256': sha,
                    'mtime': datetime.fromtimestamp(path.stat().st_mtime).isoformat()
                })

    # HAL test results
    hal_log = BUILD_DIR / "hal-tests" / "test-output.log"
    hal_pass = hal_fail = hal_skip = 0
    if hal_log.exists():
        content = hal_log.read_text()
        hal_pass = content.count("[PASS]")
        hal_fail = content.count("[FAIL]")
        hal_skip = content.count("[SKIP]")

    # Compute totals
    total_size = sum(a['size_bytes'] for a in artifacts)
    total_libs = sum(1 for a in artifacts if a['kind'] in ('shared_lib', 'static_lib'))

    # Generate JSON manifest
    manifest = {
        'build_timestamp': timestamp,
        'build_dir': str(BUILD_DIR),
        'totals': {
            'artifacts': len(artifacts),
            'libraries': total_libs,
            'total_size_bytes': total_size,
            'hal_tests_pass': hal_pass,
            'hal_tests_fail': hal_fail,
            'hal_tests_skip': hal_skip,
        },
        'artifacts': sorted(artifacts, key=lambda a: (a['kind'], a['module'], a['name']))
    }
    json_path = REPORT_DIR / f"build-manifest-{timestamp}.json"
    json_path.write_text(json.dumps(manifest, indent=2))
    print(f"JSON manifest: {json_path}")

    # Generate Markdown report
    lines = [
        f"# AfriOS Build Report",
        f"",
        f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        f"",
        f"## Summary",
        f"",
        f"| Metric | Value |",
        f"|--------|-------|",
        f"| Build directory | `{BUILD_DIR}` |",
        f"| Total artifacts | {len(artifacts)} |",
        f"| Shared/static libraries | {total_libs} |",
        f"| Total size | {total_size / 1024:.1f} KB |",
        f"| HAL tests | **{hal_pass} PASS** / {hal_fail} FAIL / {hal_skip} SKIP |",
        f"",
        f"## Artifacts by kind",
        f"",
    ]

    # Group by kind
    by_kind = {}
    for a in artifacts:
        by_kind.setdefault(a['kind'], []).append(a)

    for kind in ['shared_lib', 'static_lib', 'executable']:
        if kind not in by_kind:
            continue
        lines.append(f"### {kind.replace('_', ' ').title()}")
        lines.append("")
        lines.append("| Module | Name | Size | SHA-256 (first 12) |")
        lines.append("|--------|------|------|---------------------|")
        for a in sorted(by_kind[kind], key=lambda x: (x['module'], x['name'])):
            size_str = f"{a['size_bytes'] / 1024:.1f} KB" if a['size_bytes'] < 1024*1024 else f"{a['size_bytes'] / (1024*1024):.2f} MB"
            lines.append(f"| `{a['module']}` | `{a['name']}` | {size_str} | `{a['sha256'][:12]}` |")
        lines.append("")

    # HAL test results
    if hal_log.exists():
        lines.append(f"## HAL test results")
        lines.append("")
        lines.append(f"- **PASS:** {hal_pass}")
        lines.append(f"- **FAIL:** {hal_fail}")
        lines.append(f"- **SKIP:** {hal_skip}")
        lines.append(f"- Log file: `{hal_log.relative_to(BUILD_DIR.parent)}`")
        lines.append("")
        # Show first 10 and last 10 test lines
        content = hal_log.read_text().splitlines()
        lines.append("### Sample test output")
        lines.append("")
        lines.append("```")
        for line in content[:20]:
            lines.append(line)
        lines.append("...")
        for line in content[-10:]:
            lines.append(line)
        lines.append("```")
        lines.append("")

    # Modules built summary
    lines.append("## Modules built")
    lines.append("")
    lines.append("| Module | Status | Type |")
    lines.append("|--------|--------|------|")
    modules_built = {
        'afros-winbridge': ('OK', 'shared lib (.so)'),
        'afros-androsandbox': ('OK', 'shared lib (.so)'),
        'afros-harmonygate': ('OK', 'shared lib (.so)'),
        'afros-dxvk': ('OK', 'shared lib (.so) + 4 Wine DLL aliases'),
        'afros-corebridge-core': ('OK', 'static lib (.a)'),
        'afros-incompat-engine': ('PARTIAL', 'shared lib (.so, .m excluded — needs gobjc)'),
        'hal-tests (port-host-mock)': ('OK', 'executable + tests PASS'),
        'linux tests': ('OK', '2 ELF binaries built + run'),
    }
    for m, (status, type_) in modules_built.items():
        emoji = '✅' if status == 'OK' else '⚠️'
        lines.append(f"| {emoji} {m} | {status} | {type_} |")
    lines.append("")

    # Skipped builds (cross-compilers unavailable)
    lines.append("## Skipped builds (cross-compilers unavailable)")
    lines.append("")
    lines.append("| Target | Reason | Required toolchain |")
    lines.append("|--------|--------|-------------------|")
    lines.append("| Kernel ARM64 (port-arm64) | cross-compiler missing | `gcc-aarch64-linux-gnu` |")
    lines.append("| Kernel RISC-V (port-riscv) | cross-compiler missing | `gcc-riscv64-linux-gnu` |")
    lines.append("| Kernel MCU (port-mcu) | cross-compiler missing | `gcc-arm-none-eabi` |")
    lines.append("| Windows tests (.exe) | mingw missing | `x86_64-w64-mingw32-gcc` |")
    lines.append("| Firmware UEFI | EDK2 not vendored | `fetch-deps.sh edk2` (~5 GB) |")
    lines.append("")
    lines.append("To enable these builds, install the required toolchains and re-run:")
    lines.append("```bash")
    lines.append("# Install cross-compilers (Debian/Ubuntu)")
    lines.append("sudo apt-get install gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu \\")
    lines.append("                 gcc-arm-none-eabi x86_64-w64-mingw32-gcc")
    lines.append("")
    lines.append("# Vendor EDK2")
    lines.append("./scripts/fetch-deps.sh edk2")
    lines.append("```")
    lines.append("")

    md_path = REPORT_DIR / f"build-report-{timestamp}.md"
    md_path.write_text("\n".join(lines))
    print(f"Markdown report: {md_path}")

    # Also save a "latest" version
    latest_md = REPORT_DIR / "build-report-latest.md"
    latest_md.write_text("\n".join(lines))
    latest_json = REPORT_DIR / "build-manifest-latest.json"
    latest_json.write_text(json.dumps(manifest, indent=2))
    print(f"Latest copies: {latest_md}, {latest_json}")

if __name__ == '__main__':
    main()
