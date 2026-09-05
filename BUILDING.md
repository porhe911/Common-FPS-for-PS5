# Building v1.1.0 (PARITY TEST31)

## GitHub Actions

Open **Actions → PS5 Source Build → Run workflow**. The workflow prepares the
pinned dependencies, runs host tests, builds the PS5 controller and verifies
the output wrapper and diagnostic boundary.

The downloadable artifact contains:

```text
Common_FPS_PS5_v1.1.0_PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.elf
Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.plugin
SHA256SUMS.txt
RESOLVED_BUILD_DEPENDENCIES.txt
```

## Local PS5 build

On a POSIX host with the required build tools:

```bash
bash ./scripts/prepare_ps5_deps.sh
bash ./scripts/ps5_source_build.sh
```

`scripts/ps5_source_build.sh` invokes
`tools/verify_test31_artifact.py`. The reference build produced:

```text
2db81cb2f896eb5310e22715226cc065e1b2c22304f6376c0fdbac5ce32b9f3a  ELF
fefdab4c49fc58eddfd798697d1479818367db67d6543f1994809d3002fcbc19  plugin
```

## Host tests

```bash
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 tests/test_plugin_wrapper.py
```

## Runtime limitation

TEST31 samples FPS in the background and sends integer state to the embedded
ShellUI renderer. The first valid result for each game PID is also logged.
