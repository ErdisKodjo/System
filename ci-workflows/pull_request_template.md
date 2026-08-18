<!--
  AfriOS pull-request template.
  Please fill in every section. Sections marked "REQUIRED" must be non-empty
  before requesting review; empty PRs will be blocked by the worklog-validation
  CI job.
-->

## Summary

<!-- REQUIRED: 1–3 sentences describing *what* this PR changes and *why*. -->

## Architecture(s) affected

<!-- REQUIRED: check all that apply. Delete lines that don't apply. -->

- [ ] ARM64 (`arm64`)
- [ ] x86_64 (`x86_64`)
- [ ] RISC-V (`riscv`)
- [ ] MCU / Cortex-M (`mcu`)
- [ ] Firmware (EDK2 / UEFI)
- [ ] Host-only (test / docs / CI)
- [ ] All architectures (no arch-specific code changed)

## Subsystem(s) affected

<!-- REQUIRED: check all that apply. -->

- [ ] `afros-core` (kernel / HAL)
- [ ] `afros-corebridge-core` (orchestrator)
- [ ] `afros-winbridge` (Windows/Wine compat)
- [ ] `afros-androsandbox` (Android compat)
- [ ] `afros-incompat-engine` (iOS/macOS compat)
- [ ] `afros-harmonygate` (HarmonyOS compat)
- [ ] `afros-dxvk` (DirectX → Vulkan)
- [ ] `FirmwareHybride` (UEFI firmware)
- [ ] `afros-network` / `afros-storage` / `afros-power-management`
- [ ] Build system (CMake / scripts)
- [ ] CI/CD (`.github/`)
- [ ] Documentation

## Tests run

<!-- REQUIRED: list the manual or CI checks you ran. -->

- [ ] `python3 scripts/ci-syntax-check.py <path>` — passes locally
- [ ] `cmake -B build -S AfriOS/AfriOS/OS -DAFROS_PORT=<arch>` — configures
- [ ] `cmake --build build -j$(nproc)` — builds
- [ ] `afros-core/Kernel/hal/tests/hal_smoke_test.c` — passes on host
- [ ] New unit test(s) added (list paths below)
- [ ] No tests run (justification: <!-- e.g. docs-only change -->)

```
# Paste test output or paths to new tests here:
```

## Breaking changes?

<!-- REQUIRED. If YES, describe the migration path. -->

- [ ] No — fully backward compatible
- [ ] Yes — see migration notes below

```
<!-- Migration notes (if any) -->
```

## Worklog updated?

<!-- REQUIRED: every PR that adds/modifies a non-trivial amount of code
     must append a `---` section to AfriOS/WORKLOG-COMPLETION.md. -->

- [ ] Yes — I appended a new section with `Task ID`, `Agent`, `Task`,
      `Work Log`, and `Stage Summary`
- [ ] No — this PR is too small to warrant a worklog entry (justification: <!-- … -->)

## Checklist

- [ ] Code follows the existing style (HAL op-table pattern, French comments
      where the surrounding file uses French, `kprintf` not `printf` inside
      the freestanding kernel)
- [ ] Status codes use `AFROS_SUCCESS` / `AFROS_ERROR_*` from
      `afros-core/Kernel/hal/include/afros_types.h`
- [ ] No new `printf`/`stdio.h` introduced inside `afros-core/Kernel/`
      (use `kprintf` from `hal/include/kprintf.h`)
- [ ] New `CMakeLists.txt` entries follow the modern target-based style
      (`target_link_libraries`, `POSITION_INDEPENDENT_CODE ON`)
- [ ] No files modified outside this PR's assigned scope (per the multi-agent
      protocol)
- [ ] No `git commit` / `git push` run by an agent (only the parent agent
      commits)

## Additional context

<!-- Anything reviewers should know — screenshots, benchmarks, links to
     design docs, follow-up issues, etc. -->
