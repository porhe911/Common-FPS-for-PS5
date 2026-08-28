# Building Common FPS for PS5

## Recommended: GitHub Actions

Windows users do not need WSL.

Upload this repository to GitHub, then:

```text
Actions
-> PS5 Source Build
-> Run workflow
```

The workflow downloads the pinned PS5 Payload SDK and GPL upstream source
dependencies, runs source gates/tests, and prepares the PS5 build workspace.

When the cross-build target succeeds, the workflow uploads:

```text
Common_FPS_PS5_v1.1.0.elf
Common_FPS_PS5_etaHEN_v1.1.0.plugin
Common_FPS_ShellUI_v1.1.0.elf
```

as workflow artifacts.

## Host tests

On a normal C++17 environment:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Also run:

```bash
python3 tools/check_v1_source_gate.py
python3 tests/test_plugin_wrapper.py
```

## Local PS5 build

A POSIX build host is optional. The build environment is defined by
`.github/workflows/ps5-source-build.yml`.

Pinned dependencies are listed in:

```text
DEPENDENCIES.lock.json
```

## Release rule

A successful compiler run is not enough to declare a stable PS5 release.

The produced binaries must also pass `docs/HARDWARE_TEST_PLAN_FW960.md`.

For the current Safe Autoload Stage A hardware procedure, see `docs/AUTOLOAD_STAGE_A_HW_PROBE.md`.
