---
name: Feature request
about: Propose a new feature or enhancement for AfriOS
title: "[FEAT] <short one-line summary>"
labels: ["enhancement", "triage"]
assignees: []
---

## Summary

<!-- 1–3 sentences: what feature / enhancement and why it matters. -->

## Motivation

<!-- What problem does this solve? Who benefits? Include concrete use
     cases (e.g. "Android apps that depend on … currently fail because …"). -->

## Proposed solution

<!-- Describe the high-level approach. Mention any new APIs, files, or
     subsystems that need to be created. If you've already sketched an
     implementation, link to the branch / commit. -->

## Alternatives considered

<!-- What else could solve this? Why did you reject those options? -->

## Affected component(s)

- [ ] Kernel (`afros-core`)
- [ ] HAL (`afros-core/Kernel/hal/`)
- [ ] Firmware (EDK2 / UEFI)
- [ ] CoreBridge orchestrator (`afros-corebridge-core`)
- [ ] WinBridge (Windows/Wine compat)
- [ ] AndroSandbox (Android compat)
- [ ] Incompat-engine (iOS/macOS compat)
- [ ] HarmonyGate (HarmonyOS compat)
- [ ] DXVK (DirectX → Vulkan)
- [ ] Network / Storage / Power-management
- [ ] Build system (CMake)
- [ ] CI/CD (`.github/`)
- [ ] Documentation
- [ ] Other: <!-- … -->

## Architecture(s) of interest

- [ ] ARM64
- [ ] x86_64
- [ ] RISC-V
- [ ] MCU / Cortex-M
- [ ] Host (test/sim)
- [ ] All

## API / ABI impact

- [ ] No new APIs
- [ ] New public API (header in `include/`)
- [ ] Breaking change to existing API — describe migration:

## Performance / memory impact

<!-- Rough estimate of binary size, RAM, or CPU overhead. "Unknown" is OK
     at this stage. -->

## Testing plan

<!-- How would this be tested? New unit tests? Integration tests?
     Manual smoke on real hardware? -->

## Open questions

<!-- Anything you'd like reviewers / maintainers to weigh in on before
     implementation starts. -->

## Additional context

<!-- Links to relevant issues, design docs, upstream references, etc. -->
