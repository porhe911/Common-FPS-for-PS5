# Building Common FPS for PS5 v1.2.5

## DCE-only compatibility change (v1.2.5)

v1.2.5 disables game-process VideoOut memory scans/reads at compile time
for **all firmware**, relying on DCE flip counters or HEN-shared FPS instead.
The user confirmed the v1.2.5 test build runs with the GTA IV PS5 native
port using the game's 120 FPS setting on FW reported as 9.0 (SDK
`0x09050005`) and a 60 Hz TV, with overlay present and no repeat crash.
See `release/TEST_NOTES_v1.2.5_SAFE_DCE.md`. Hardware testing beyond this
specific firmware/game/display remains outstanding.

## GitHub Actions

Open **Actions → PS5 Source Build → Run workflow**.

The workflow prepares pinned dependencies, runs host regression tests, builds
the controller and embedded ShellUI component, packages the etaHEN plugin,
verifies the release boundary and generates checksums.

The source-build artifact contains:

```text
Common_FPS_PS5_v1.2.5.elf
Common_FPS_PS5_etaHEN_v1.2.5.plugin
Common_FPS_ShellUI_v1.2.5.elf
SHA256SUMS.txt
RESOLVED_BUILD_DEPENDENCIES.txt
```

The separate ShellUI ELF is a build/debug artifact. It is already embedded in
the controller. Public releases contain only the controller ELF and plugin.

## Local PS5 build

On a POSIX host with the required build tools:

```bash
bash ./scripts/prepare_ps5_deps.sh
bash ./scripts/ps5_source_build.sh
```

The build script verifies the artifacts and writes `dist/SHA256SUMS.txt`.
Dependencies are pinned in `DEPENDENCIES.lock.json`.

## Host tests

```bash
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 tools/check_v1_source_gate.py
python3 tests/test_plugin_wrapper.py
python3 tests/test_safe_dce_guard.py
npm install --prefix build-ui --no-save playwright@1.62.1
./build-ui/node_modules/.bin/playwright install chromium
NODE_PATH="$PWD/build-ui/node_modules" node tests/test_settings_ui.js ./build-host/common_fps_settings_server ./build-host/settings-preview.png
```

The host suite includes FPS filtering, lifecycle/autoload handling, settings
persistence, bounded session logs, protected ShellUI code access and rendering
against a collecting Mono/PUI mock. Browser integration exercises the real
HTTP worker with controller/keyboard/native events and saved settings.

## Runtime model and validation

The v1.2.5 controller tracks the active game and sends integer FPS from
the DCE/HEN-shared source to the embedded ShellUI renderer. It follows game
PID changes and supported ShellUI lifecycle changes. The controller's FPS
sampler neither reads nor writes game-process memory in v1.2.5, but ShellUI
bootstrap still accesses its own target process for renderer injection.
Displayed values are capped at 120 FPS; the DCE path is not a proven measure
of game-engine rendering FPS above the output refresh rate.

The display settles for five seconds, then uses the median of three samples.
Large rises require three observations spanning at least two seconds;
sustained 120 FPS and individual drops remain visible. PID/source changes or
lost sampling restart settling. This filters numbers rather than detecting a
loading screen.

The default font size is 22 with a 4-unit bottom margin. Rendering uses a
logical 1920×1080 canvas. Temporary managed values are pinned, styles are
updated only when changed and widgets are recreated after a RootWidget change.
Other PUI canvas sizes remain unverified on hardware.

Settings use an embedded loopback-only service on port 39029 and atomic saves
in `/data/CommonFPS/config.ini`. Both logs reset on activation and are bounded
to 64 KiB each. Controller and renderer use wire version 2 (44 bytes); reboot
before upgrading so that an older resident renderer is unloaded.

The user confirmed that v1.2.5 works in GTA IV with the 120 FPS game
setting on the affected FW 9.0 console (SDK 0x09050005) and 60 Hz TV,
after v1.2.4 had caused a crash. The exact mechanism is not proven; this
validates the specific hardware workflow, not measured 120 Hz output or
other firmware. See [v1.2.5 evidence](docs/evidence/FW900_V1_2_5_GTA4_120_MODE_CONFIRMED_20261121.md)
and [release notes](release/RELEASE_NOTES_v1.2.5.md).
FW 11.60 was previously tested on v1.2.3 on 2026-10-16.

## Stable publication

Merging the `release/PUBLISH_v1.2.5` marker into `main` triggers
**Publish Common FPS v1.2.5**. It builds and verifies the release, creates
an immutable tag, uploads only the source-built ELF/plugin, downloads and
compares them, and marks the release Latest. No prior tag or release assets
are replaced. The user-authorized release is hardware-confirmed on the
reported FW 9.0 console only; other platforms/firmwares remain unverified
for v1.2.5.
