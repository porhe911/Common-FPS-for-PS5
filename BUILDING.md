# Building Common FPS for PS5 v1.2.3 RC2

## GitHub Actions

Open **Actions → PS5 Source Build → Run workflow**.

The workflow:

1. prepares the pinned PS5 dependencies;
2. runs host regression tests;
3. builds the PS5 controller and ShellUI component;
4. packages the etaHEN plugin;
5. verifies the release boundary;
6. generates `SHA256SUMS.txt`.

The downloadable artifact contains:

```text
Common_FPS_PS5_v1.2.3.elf
Common_FPS_PS5_etaHEN_v1.2.3.plugin
Common_FPS_ShellUI_v1.2.3.elf
SHA256SUMS.txt
RESOLVED_BUILD_DEPENDENCIES.txt
```

## Local PS5 build

On a POSIX host with the required build tools:

```bash
bash ./scripts/prepare_ps5_deps.sh
bash ./scripts/ps5_source_build.sh
```

The build script also runs the PS5 artifact verifier and writes current
checksums to `dist/SHA256SUMS.txt`.

## Host tests

```bash
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 tests/test_plugin_wrapper.py
```

## Runtime model

The v1.2.3 RC5 controller tracks the active game process, samples integer FPS,
sends state to the embedded ShellUI component, follows game PID changes and
recovers the overlay after supported ShellUI lifecycle changes.

The default overlay font size is 24.

RC4's FW 11.60 log confirms checked MDBG code reads succeed after PT_IO
EFAULT, but the one-byte guard write still fails through PT_IO. RC5 writes
only that verified guard byte using checked MDBG. It snapshots the original
VM protection flags, enables writing temporarily, restores and verifies the
exact flags before detach, and retains that snapshot through rollback errors.
The SDK changes the containing VM entry, not an isolated hardware page.
Expected-byte checks, full code verification and credential restoration stay
mandatory. Previous firmware paths remain unchanged. Firmware 11.60 overlay
operation remains experimental and is not hardware-confirmed. See
[RC5 notes](release/RELEASE_NOTES_v1.2.3_RC5.md) and
[hardware evidence](docs/evidence/FW1160_RC4_GUARD_WRITE_20261014.md).
The distinct [Preflight](release/FW1160_PREFLIGHT_v0.1_RU.md) intentionally
contains no FPS renderer and is not the full counter.
