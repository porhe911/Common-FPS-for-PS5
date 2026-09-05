# Building v1.1.0

## GitHub Actions

Open **Actions → PS5 Source Build → Run workflow**. The workflow prepares the
pinned dependencies, runs host tests, builds the PS5 controller and verifies
the output wrapper and diagnostic boundary.

The downloadable artifact contains:

```text
Common_FPS_PS5_v1.1.0.elf
Common_FPS_PS5_etaHEN_v1.1.0.plugin
Common_FPS_ShellUI_v1.1.0.elf
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
`tools/verify_v1_1_0_artifact.py`. The reference build produced:

```text
4f544fa00f7a430e64c4c8d0ed42d0463d2370c81dabd9141599c27c4f3f99d6  ELF
39333081ecd93ade60d1b75fb0032a1e996fcf17ad47a9adfc0290591596e44e  plugin
7880aec891cb95cc860753d5a3fed1dfbb23caf526b6106275c3b2cc02b8e465  ShellUI renderer
```

## Host tests

```bash
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 tests/test_plugin_wrapper.py
```

## Runtime limitation

The v1.1.0 controller samples FPS in the background and sends integer state to the embedded
ShellUI renderer. The first valid result for each game PID is also logged.
