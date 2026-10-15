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

The v1.2.3 RC9 controller tracks the active game process, samples integer FPS,
sends state to the embedded ShellUI component, follows game PID changes and
recovers the overlay after supported ShellUI lifecycle changes.

The default font size is 22 with a 4px bottom margin (about 9 logical pixels
lower than RC6). The display waits five seconds after valid sampling starts,
then uses the median of the latest three observations. Large rises require
three observations spanning at least two seconds. Sustained 120 FPS remains
supported; individual drops and recovery from a short drop remain visible.
Changing PID/source or losing sampling restarts settling. This is numerical
filtering, not detection of a game's loading-screen state.

On 2026-10-15 the user confirmed RC7 works perfectly on FW 11.60. They then
confirmed RC8's Media shortcut works, but reported a game freeze after changing
styles and controller activation failures in the page. The controller log
proves successful registration/bootstrap/guard restoration, not the hang cause.
See [RC8 evidence](docs/evidence/FW1160_RC8_SETTINGS_FREEZE_20261015.md).

RC9 keeps the sampler, startup filter, installer and stopped-process injector.
Its page has explicit palettes and unified native/keyboard/Gamepad activation,
including legacy numeric buttons and duplicate event suppression. Successful
saves are logged after the atomic store update.

The renderer pins all temporary UIFont/UIColor owners, strings and new labels.
It applies initial styles once, changes fonts only on actual size changes and
forgets cached children when RootWidget changes within the same Scene.
Width/Height managed getters are removed. Rendering uses the known logical
1920×1080 canvas pending investigation; no physical screen diagonal is queried.
A Linux host test compiles the actual renderer with a fake collecting Mono/PUI
runtime to test lifetimes, selective updates, root changes and failed setters.
Console hang recovery and other PUI scene sizes are still unverified.

Both components use wire version 2 (44 bytes). Reboot when upgrading from RC8
to unload its resident renderer. Settings stay in /data/CommonFPS/config.ini;
HTTP remains loopback-only and logs remain 64 KiB each/reset on activation.
See [RC9 notes](release/RELEASE_NOTES_v1.2.3_RC9.md).
The distinct [Preflight](release/FW1160_PREFLIGHT_v0.1_RU.md) intentionally
contains no FPS renderer and is not the full counter.
