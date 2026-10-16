# Building Common FPS for PS5 v1.2.3

## GitHub Actions

Open **Actions → PS5 Source Build → Run workflow**.

The workflow prepares pinned dependencies, runs host regression tests, builds
the controller and embedded ShellUI component, packages the etaHEN plugin,
verifies the release boundary and generates checksums.

The source-build artifact contains:

```text
Common_FPS_PS5_v1.2.3.elf
Common_FPS_PS5_etaHEN_v1.2.3.plugin
Common_FPS_ShellUI_v1.2.3.elf
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
npm install --prefix build-ui --no-save playwright@1.62.1
./build-ui/node_modules/.bin/playwright install chromium
NODE_PATH="$PWD/build-ui/node_modules" node tests/test_settings_ui.js ./build-host/common_fps_settings_server ./build-host/settings-preview.png
```

The host suite includes FPS filtering, lifecycle/autoload handling, settings
persistence, bounded session logs, protected ShellUI code access and rendering
against a collecting Mono/PUI mock. Browser integration exercises the real
HTTP worker with controller/keyboard/native events and saved settings.

## Runtime model and validation

The v1.2.3 controller tracks the active game, samples integer FPS and sends
state to the embedded ShellUI renderer. It follows game PID changes and
supported ShellUI lifecycle changes. The sampler does not write game memory.

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

The user reported RC9 working on FW 11.60 on 2026-10-16 and authorized stable
publication. v1.2.3 changes version labels and release metadata, preserving
RC9's functional code. See [confirmation](docs/evidence/FW1160_RC9_CONFIRMED_20261016.md)
and [release notes](release/RELEASE_NOTES_v1.2.3.md).
The earlier [RC8 freeze report](docs/evidence/FW1160_RC8_SETTINGS_FREEZE_20261015.md)
is retained as historical evidence; its exact failure cause was not proven.

## Stable publication

Merging the v1.2.3 publication marker into `main` triggers
**Publish Common FPS v1.2.3**. It builds and verifies the release, creates the
new tag without moving existing tags, uploads only ELF/plugin to a draft,
downloads and compares both files, then publishes it as the latest release.
A retry checks existing tag and asset identities instead of replacing them.
