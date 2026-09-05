# Building the PARITY TEST20 source

## GitHub Actions

Open **Actions → PS5 Source Build → Run workflow**. The workflow prepares the
pinned dependencies, runs host tests, builds the PS5 controller and verifies
both output hashes.

The downloadable artifact contains:

```text
Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf
Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.plugin
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
`tools/verify_test20_repro.py`. Success requires these exact digests:

```text
b791baf6f85063d0c7ec57eebcbf60116dff58dfc4ec74aa6d9dedcc1ecc32ef  ELF
7bdcc16cc582bd89c7f1680471436ef58fdc53bab159c7e36278a5e8f1c8fa49  plugin
```

## Host tests

```bash
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 tests/test_plugin_wrapper.py
```

## Runtime limitation

This exact TEST20 build samples FPS in the background and logs only the first
valid result for each game PID. It intentionally has no on-screen overlay.
