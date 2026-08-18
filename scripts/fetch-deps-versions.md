# AfriOS External Dependencies — Pinned Versions

This document is the **authoritative source of truth** for the external
dependency versions that AfriOS is built and tested against. The actual
machine-readable pins live in [`fetch-deps.sh`](fetch-deps.sh) (the `DEPS`,
`REPOS`, `SUBDIRS` associative arrays). The two files **must** be kept in
sync — use [`update-dep.sh`](update-dep.sh) to bump a pin and it will update
both at once.

> Why pin? Without pinned versions, the same commit of AfriOS can produce
> different binaries on two machines just because the upstream tag moved or
> the system packages were upgraded. Pinning makes the build reproducible
> and the supply chain auditable.

## Pinned dependency table

| Name | Version (pinned) | Release date | Repository | Approx. size (shallow) | License | Why AfriOS needs it | Tested with AfriOS commit |
|---|---|---|---|---|---|---|---|
| `edk2` | `edk2-stable202408` | 2024-08 | https://github.com/tianocore/edk2.git | ~5 GB (with submodules) | BSD-2-Clause-Patent | UEFI firmware reference implementation. AfriOS `FirmwareHybride/edk2/HybridFirmwarePlatformPkg/` builds on top of the EDK2 package model (`.dsc`/`.dec`/`.inf`). Required to produce the `.fd` firmware images the platform boots from. | _pending first green firmware-build CI run_ |
| `wine` | `wine-9.0` | 2024-01 | https://gitlab.winehq.org/wine/wine.git | ~400 MB | LGPL-2.1 (Wine License) | Windows compatibility layer. `afros-winbridge` wraps Wine's PE loader, syscall translator, registry hive format and COM runtime — all of which mirror Wine's design. The pinned tag is the reference against which the winbridge shims were authored. | _pending first green compat-layer-build CI run_ |
| `art` | `android-14.0.0_r1` | 2023-10 | https://android.googlesource.com/platform/art | ~200 MB | Apache-2.0 | Android Runtime (ART): the `dex2oat` compiler, class linker and JIT that `afros-androsandbox` mirrors. Pinned to the Android 14 release so the sandbox exposes the same ART API surface as a real Android 14 device. | _pending first green compat-layer-build CI run_ |
| `darling` | `0.1.20240301` | 2024-03 | https://github.com/darlinghq/darling.git | ~100 MB | MIT ( Darling ) + Apple APSL components (gated) | macOS/iOS translation layer. `afros-incompat-engine` reuses Darling's Mach-O loader, Objective-C runtime bridge and bundle manager design. The pin tracks Darling's 2024 development snapshot. | _pending first green compat-layer-build CI run_ |
| `harmony` | `5.0.0-Release` | 2024-10 | https://gitlab.com/harmonyos/release/ohos-release | ~500 MB | Apache-2.0 (OpenHarmony) | HarmonyOS (OpenHarmony) SDK release. `afros-harmonygate` mirrors the OHOS ability runtime, distributed softbus and liteos kernel abstractions. Pinned to the OHOS 5.0 stable release. | _pending first green compat-layer-build CI run_ |
| `vulkan` | `v1.3.290` | 2024-02 | https://github.com/KhronosGroup/Vulkan-Headers.git | ~10 MB | Apache-2.0 (Khronos) | Vulkan header set. `afros-dxvk` includes `vulkan/vulkan.h` and the Vulkan dispatch tables; the pin tracks Vulkan 1.3.290 (API + loader headers). | _pending first green compat-layer-build CI run_ |
| `glslang` | `14.2.0` | 2024-05 | https://github.com/KhronosGroup/glslang.git | ~30 MB | BSD-3-Clause (glslang) | Khronos reference GLSL/HLSL → SPIR-V compiler. `afros-dxvk`'s HLSL frontend (`src/hlsl/`) delegates SPIR-V generation to glslang; pinned to the 14.2 release that introduced the HLSL refactor `afros-dxvk` depends on. | _pending first green compat-layer-build CI run_ |
| `mesa` | `mesa-24.0.3` | 2024-03 | https://gitlab.freedesktop.org/mesa/mesa.git | ~150 MB | MIT (Mesa) | Mesa 3D graphics library — provides the OpenGL fallback headers and software rasteriser paths used when no Vulkan driver is available. Optional: only needed for the OpenGL fallback build flavour. | _pending first green compat-layer-build CI run (optional)_ |
| `iconv` | `v1.17` | 2022-06 | https://git.savannah.gnu.org/git/libiconv.git | ~5 MB | LGPL-2.1+ (GNU libiconv) | GNU libiconv — charset conversion (`libintl`/`libiconv`). Used by the AfriOS locale layer and by Wine's Unicode conversion paths. Optional: most Linux distros ship an equivalent glibc iconv. | _pending first green compat-layer-build CI run (optional)_ |

## Field notes

- **Version (pinned)**: the exact tag or commit hash passed to
  `git clone --branch <pin>`. Bump via `update-dep.sh`, never by hand.
- **Approx. size (shallow)**: footprint of a `--depth 1` clone on disk
  after the post-clone hook (EDK2 submodules add ~4 GB on top of the
  top-level ~1 GB). Full-history clones are several times larger — that's
  why shallow clones are the default.
- **License**: the upstream project's license. AfriOS does **not** modify
  the contents of `external/<name>/`; vendoring at a pinned commit keeps
  the upstream license intact. See [`external/README.md`](../external/README.md)
  for the security & licensing notes.
- **Tested with AfriOS commit**: populated by the first CI run that builds
  successfully against this pin. Until then the cell reads
  `_pending first green … CI run_`. When you bump a pin, run the relevant
  CI job and replace the cell with the AfriOS commit SHA that produced a
  green build.

## Update policy

1. Bump **at most one** dependency per PR — bisecting a regression across a
   multi-dep bump is painful.
2. Use `./scripts/update-dep.sh <name> <new-tag-or-commit>`; it edits
   `fetch-deps.sh`, re-fetches, and updates the row above.
3. After the bump, run the relevant CI job (firmware-build for `edk2`,
   compat-layer-build for the others). On green, record the AfriOS commit
   in the "Tested with AfriOS commit" column.
4. Old pins should be mentioned in the PR description with a link to the
   upstream changelog so reviewers can sanity-check the diff.

## See also

- [`fetch-deps.sh`](fetch-deps.sh) — the fetcher script (source of truth for pins)
- [`check-deps.sh`](check-deps.sh) — verify presence + pinned versions
- [`update-dep.sh`](update-dep.sh) — bump a pin and re-fetch
- [`../external/README.md`](../external/README.md) — notes on the `external/` directory
