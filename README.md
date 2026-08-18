# AfriOS

AfriOS is a multi-platform operating-system stack — micro-kernel, hybrid
firmware, Android / Windows / iOS / HarmonyOS compatibility layers, GPU
bridge, runtime orchestrator — targeting ARM64 first, then x86_64, RISC-V
and MCU.

This is the **top-level** repo README. Detailed documentation lives under
[`AfriOS/AfriOS/OS/afros-docs/`](AfriOS/AfriOS/OS/afros-docs/) and
[`AfriOS/FirmwareHybride/docs/`](AfriOS/FirmwareHybride/docs/).

## Quick Start

```bash
# One-shot setup (checks deps, installs workflows, runs tests)
./scripts/setup.sh --all

# Or run individual phases
./scripts/setup.sh --check-deps
./scripts/setup.sh --syntax-check
./scripts/setup.sh --hal-tests
./scripts/setup.sh --compat-tests

# Run all tests (syntax + HAL + compat + cross-link + CMake configure)
./scripts/run-all-tests.sh
```

The HAL tests run against the **host-mock port**
([`AfriOS/AfriOS/OS/afros-core/Kernel/ports/port-host-mock/`](AfriOS/AfriOS/OS/afros-core/Kernel/ports/port-host-mock/)),
which provides userspace-safe stubs for every `arch_*_ops` table so the
test runner can execute on a standard Linux CI runner — no bare-metal or
QEMU required. Output is captured to
[`tests/results/hal-tests-<timestamp>.log`](tests/results/).

See `docs/` for detailed documentation:

- [`AfriOS/AfriOS/OS/afros-docs/Architecture.md`](AfriOS/AfriOS/OS/afros-docs/Architecture.md)
- [`AfriOS/AfriOS/OS/afros-docs/Testing.md`](AfriOS/AfriOS/OS/afros-docs/Testing.md)
- [`AfriOS/AfriOS/OS/afros-core/Kernel/hal/docs/architecture.md`](AfriOS/AfriOS/OS/afros-core/Kernel/hal/docs/architecture.md)
- [`AfriOS/AfriOS/OS/afros-core/Kernel/ports/README.md`](AfriOS/AfriOS/OS/afros-core/Kernel/ports/README.md)

## Repository layout

```
System/
├── AfriOS/
│   ├── AfriOS/OS/                 # the operating-system stack
│   │   ├── afros-core/            # micro-kernel + HAL + drivers
│   │   │   └── Kernel/
│   │   │       ├── hal/           # hardware abstraction layer
│   │   │       └── ports/         # arch ports (x86_64, arm64, riscv, mcu, host-mock)
│   │   ├── afros-corebridge-core/ # runtime orchestrator
│   │   ├── afros-winbridge/       # Windows / Wine compatibility
│   │   ├── afros-androsandbox/    # Android runtime
│   │   ├── afros-incompat-engine/ # iOS / macOS via Darling
│   │   ├── afros-harmonygate/     # HarmonyOS compatibility
│   │   └── afros-dxvk/            # DirectX → Vulkan
│   └── FirmwareHybride/           # UEFI / EDK2 hybrid firmware
├── ci-workflows/                  # canonical GitHub Actions / community-health files
├── scripts/                       # one-shot setup + per-suite test runners
└── tests/                         # compatibility test manifests + harness
    └── results/                   # test logs + reports (gitignored)
```

## Tooling

| Tool | Required for |
|---|---|
| `gcc`, `g++` | syntax-check, HAL tests, host-mock port |
| `python3` | `ci-syntax-check.py`, `compat-test-harness.py` |
| `cmake`, `make` | CMake build path (optional — direct `gcc` works for HAL tests) |
| `git` | `install-workflows.sh`, `fetch-deps.sh` |
| `arm-none-eabi-gcc` | port-mcu link test (optional) |
| `riscv64-linux-gnu-gcc` | port-riscv link test (optional) |

Run `./scripts/setup.sh --check-deps` to see which are present on your host.

## License

See [`AfriOS/AfriOS/OS/LICENSE`](AfriOS/AfriOS/OS/LICENSE) and the
per-module `LICENSE` files. The codebase mixes GPL, MIT and BSD-style
licences; consult each component's `LICENSE` for specifics.
