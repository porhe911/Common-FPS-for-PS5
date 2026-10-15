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

The v1.2.3 RC7 controller tracks the active game process, samples integer FPS,
sends state to the embedded ShellUI component, follows game PID changes and
recovers the overlay after supported ShellUI lifecycle changes.

The default font size is 22 with a 4px bottom margin (about 9 logical pixels
lower than RC6). The display waits five seconds after valid sampling starts,
then uses the median of the latest three observations. Large rises require
three observations spanning at least two seconds. Sustained 120 FPS remains
supported; individual drops and recovery from a short drop remain visible.
Changing PID/source or losing sampling restarts settling. This is numerical
filtering, not detection of a game's loading-screen state.

On 2026-10-15 the user confirmed the RC6 overlay appeared on FW 11.60,
while requesting a smaller/lower overlay and reporting 89/120 FPS during
loading. RC7 retains that injector and protected guard-write path unchanged;
only presentation/defaults and release markers change. RC7 hardware testing
and broader FW 11.60 lifecycle validation remain pending. See
[RC7 notes](release/RELEASE_NOTES_v1.2.3_RC7.md) and
[RC6 confirmation](docs/evidence/FW1160_RC6_OVERLAY_20261015.md).
The distinct [Preflight](release/FW1160_PREFLIGHT_v0.1_RU.md) intentionally
contains no FPS renderer and is not the full counter.
