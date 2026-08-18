---
name: Bug report
about: Report a defect in AfriOS (kernel, firmware, compat layer, CI/CD, …)
title: "[BUG] <short one-line summary>"
labels: ["bug", "triage"]
assignees: []
---

## Summary

<!-- 1–3 sentences: what's wrong, in user observable terms. -->

## Affected component

<!-- Check all that apply. -->

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

## Architecture(s) affected

- [ ] ARM64
- [ ] x86_64
- [ ] RISC-V
- [ ] MCU / Cortex-M
- [ ] Host (test/sim)
- [ ] All

## Reproduction steps

<!-- Numbered list — be precise. Include exact commands, environment, etc. -->

1.
2.
3.

## Expected behavior

<!-- What you expected to happen. -->

## Actual behavior

<!-- What actually happened. -->

## Logs / stack trace

```
Paste relevant compiler output, dmesg, kernel log, or stack trace here.
Use ```fences```.
```

## Environment

- AfriOS version / commit: <!-- e.g. v0.1.0 or `git describe --tags` -->
- Host OS: <!-- e.g. Ubuntu 22.04 -->
- Toolchain: <!-- e.g. gcc 11.4.0, arm-none-eabi-gcc 10.3 -->
- Architecture: <!-- arm64 / x86_64 / riscv / mcu -->
- Relevant hardware: <!-- e.g. Raspberry Pi 4, STM32F407, qemu virt -->

## Regression?

- [ ] No — this never worked
- [ ] Yes — worked in <!-- commit / tag -->

## Workaround

<!-- If you found one, describe it here. -->

## Additional context

<!-- Screenshots, related issues, links, etc. -->
