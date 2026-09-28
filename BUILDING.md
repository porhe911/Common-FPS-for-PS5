# Building Common FPS for PS5 v1.2.0

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
Common_FPS_PS5_v1.2.0.elf
Common_FPS_PS5_etaHEN_v1.2.0.plugin
Common_FPS_ShellUI_v1.2.0.elf
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

The v1.2.0 controller tracks the active game process, samples integer FPS,
sends state to the embedded ShellUI component, follows game PID changes and
recovers the overlay after supported ShellUI lifecycle changes.

The default overlay font size is 24.
