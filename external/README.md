# external/ — vendored upstream dependencies

This directory holds **shallow clones** of the upstream projects that AfriOS
depends on at build time. It is **not** part of the git history of the AfriOS
repository: the `.gitignore` next to this file ignores everything except
itself and this README.

## Why this directory is not committed

The upstream trees (EDK2 alone is ~5 GB with submodules) are far too large
to live in the AfriOS repo, and re-distributing them would create licensing
and supply-chain liabilities that the AfriOS maintainers cannot reasonably
take on. Instead, the **pins** (exact tag or commit) are committed in
[`scripts/fetch-deps.sh`](../scripts/fetch-deps.sh) and
[`scripts/fetch-deps-versions.md`](../scripts/fetch-deps-versions.md);
anyone building AfriOS reproduces the exact same `external/` tree by
running the fetcher.

## How to populate it

From the repository root:

```bash
# Fetch every dependency (smallest first, EDK2 last):
./scripts/fetch-deps.sh all

# Or fetch a single dependency:
./scripts/fetch-deps.sh edk2
```

The fetcher is idempotent — re-running it on an already-correct checkout
prints `déjà à jour` and exits 0. See `./scripts/fetch-deps.sh --help` for
the full option list.

## Expected structure after fetch

```
external/
├── .gitignore          # ignores everything except itself + this README
├── README.md           # this file
├── edk2/               # TianoCore EDK2 (with submodules initialised)
├── wine/               # Wine
├── art/                # Android Runtime
├── darling/            # Darling (macOS/iOS)
├── harmony-sdk/        # OpenHarmony release
├── vulkan-headers/     # Khronos Vulkan headers
├── glslang/            # Khronos GLSL→SPIR-V compiler
├── mesa/               # Mesa 3D (optional)
└── libiconv/           # GNU libiconv (optional)
```

Sub-directory names are pinned in `scripts/fetch-deps.sh` (`SUBDIRS`
associative array). Build scripts and CMake files should reference them by
these exact names.

## Why shallow clones

Every dependency is fetched with `git clone --depth 1 --branch <pin>`. This
gives us exactly the tree at the pinned tag, without the full history.
Reasons:

1. **Disk** — full history of EDK2 is tens of GB; the shallow clone is
   ~5 GB (mostly submodules). Most contributors do not need the history.
2. **Bandwidth** — a fresh clone over a slow African mobile connection is
   the worst-case scenario this project optimises for; shallow clones cut
   transfer 10×–100×.
3. **Reproducibility** — the pinned tag maps to exactly one commit, so
   shallow and full clones produce identical working trees. The
   best-effort SHA-256 printed by `fetch-deps.sh` after each clone lets
   two developers confirm they ended up with byte-identical sources.

If you genuinely need full history (e.g. to `git blame` an upstream file),
set `FETCH_DEPS_SHALLOW=0`:

```bash
FETCH_DEPS_SHALLOW=0 ./scripts/fetch-deps.sh wine
```

## How to update a pinned dependency

Use the helper script — it edits both `fetch-deps.sh` (the machine-readable
pin) and `fetch-deps-versions.md` (the human-readable table):

```bash
./scripts/update-dep.sh wine wine-9.1
```

The helper:

1. rewrites the pin line in `scripts/fetch-deps.sh`,
2. re-fetches the dependency to validate the new tag/commit,
3. updates the row in `scripts/fetch-deps-versions.md`,
4. prints a `git diff` of `scripts/fetch-deps.sh` for review,
5. reminds you to commit the change (it does **not** commit).

After bumping a pin, run the relevant CI job (firmware-build for `edk2`,
compat-layer-build for the others). On a green run, record the AfriOS
commit SHA in the "Tested with AfriOS commit" column of
`fetch-deps-versions.md`.

## Security notes

- **Upstream ownership**: the contents of each `external/<name>/` directory
  belong to its upstream community. AfriOS does **not** patch, fork or
  modify the vendored sources — `external/` is a read-only mirror of the
  pinned upstream commit. If a patch is ever required, it should land
  upstream first and then be picked up via a pin bump, not applied
  locally.
- **Audit trail**: every pin is a tag or 40-char commit hash. The
  `fetch-deps-versions.md` table records the release date, license and
  rationale for each pin so a reviewer can sanity-check a bump.
- **Verification**: after each clone, `fetch-deps.sh` computes a
  best-effort SHA-256 of the working tree (excluding `.git/`) and prints
  it. Two developers fetching the same pin should get the same digest —
  if they don't, investigate before building.
- **No automatic updates**: this directory is never auto-bumped. Every pin
  change is a deliberate, reviewed, single-dependency PR.

## Verifying the local tree

Run the checker after fetching:

```bash
./scripts/check-deps.sh
```

It walks every expected dependency, reports `OK` / `MISSING` /
`WRONG_VERSION` per dep, and exits non-zero if any **mandatory** dep is
missing or on the wrong commit. Mandatory deps:

- Firmware build: `edk2`
- Compat-layer build: `wine`, `art`, `darling`, `harmony`, `vulkan`, `glslang`

Optional deps: `mesa`, `iconv` (only required for the OpenGL-fallback /
locale build flavours).
