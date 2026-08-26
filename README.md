# Common FPS for PS5

A lightweight, open-source real-time FPS counter for PlayStation 5.

> **Current source release:** `v1.1.0
> **Hardware-stable binary baseline:** `v1.0.0`  
> **License:** GPL-3.0-or-later

## What it does

Common FPS shows a minimal FPS overlay without patching the game's rendering
code.

Default appearance:

```text
FPS: 59
```

- integer FPS only;
- `FPS:` in purple (`#B366FF`);
- value in white;
- font size `26`;
- bottom-left by default;
- fixed position with no horizontal drift.

## Stable design inherited from v1.0.0

The source rewrite preserves the behavior that was validated on PS5 FW 9.60:

- read-only FPS acquisition;
- no game rendering hooks;
- no game-memory FPS patches;
- game PID reset/re-discovery;
- persistent `Game A -> Home -> Game B` lifecycle;
- asynchronous startup;
- ShellUI/PUI rendering;
- Scene readiness requirement;
- complete payload-args loader contract.

## FPS source

The documented stable sampler resolves a VideoOut counter through:

```text
eboot.bin
  -> libSceVideoOut.sprx
  -> base + 0x34980
  -> 7 probe records (0x18 bytes each)
  -> first enabled record
  -> root pointer
  -> root + 0x768
  -> uint32 counter
```

FPS is calculated from counter delta and monotonic elapsed time.

Only the final integer FPS value is exposed to the overlay.

## Safe etaHEN autoload

A hardware test of public `v1.0.0` found an etaHEN autoload startup race on
FW 9.60: the plugin could start before ShellUI was ready, later appear enabled
without a live renderer, and a game launch could end in a KP.

`v1.1.0-rc1` therefore includes a persistent Safe Autoload state machine:

```text
autoload
  -> return to etaHEN quickly
  -> worker stays alive
  -> wait for stable runtime/Scene readiness
  -> install renderer
  -> verify renderer health
  -> retry on failure
  -> normal FPS lifecycle
```

The game sampler is not allowed to enter its normal lifecycle while the
renderer/runtime side is unhealthy.

## Optional overlay configuration

The clean source model supports:

```ini
[overlay]
corner=bottom_left
font_size=26
margin_x=10
margin_y=10
```

Corner values:

```text
top_left
top_right
bottom_left
bottom_right
```

The compatibility default remains `bottom_left / 26`.

## Build model

This repository is intended to produce the PS5 artifacts **from source**:

```text
source
  + PS5 Payload SDK
  + pinned GPL upstream source dependencies
        |
        v
Common_FPS_PS5_v1.1.0.elf
Common_FPS_PS5_etaHEN_v1.1.0.plugin
```

No PHU ELF/SO binary blob is part of the source-built path.

### GitHub Actions

You do not need Linux or WSL on your own PC.

After uploading the repository to GitHub:

1. open **Actions**;
2. run **PS5 Source Build**;
3. download the produced build artifact.

Host tests run automatically on pushes and pull requests.

## Release status

`v1.1.0-rc1` is source-publication ready.

Do **not** mark `v1.1.0` as a hardware-stable release until:

1. the PS5 source-build workflow is green;
2. the produced ELF/plugin are tested on FW 9.60;
3. manual startup passes;
4. etaHEN autoload passes repeated cold boots;
5. `Game A -> Home -> Game B` passes repeatedly without KP.

The already tested `v1.0.0` remains the recommended binary until that gate
passes.

## Historical v1.0.0 source status

The historical `v1.0.0` binary was created during an iterative binary-analysis
and binary-patching workflow involving third-party PS5 homebrew renderer/loader
mechanisms.

The clean source tree in this repository is therefore **not claimed to be the
byte-for-byte corresponding source for historical v1.0.0**.

The purpose of `v1.1.x` is to replace that historical workflow with a true
source-built implementation.

See:

- `docs/SOURCE_STATUS.md`
- `docs/V1_0_0_PARITY_CONTRACT.md`
- `docs/AUTOLOAD_REGRESSION_FW960.md`
- `THIRD_PARTY_NOTICES.md`

## License

Common FPS-owned source is licensed under the **GNU General Public License
version 3 or later**.

SPDX:

```text
GPL-3.0-or-later
```

Third-party projects retain their own copyright and license notices.

## Disclaimer

Homebrew software for modified PlayStation 5 systems. Use at your own risk.
