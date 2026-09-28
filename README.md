# Common FPS for PS5 v1.2.0

Real-time FPS overlay for PS4 and PS5 games running on a modified PlayStation 5.

Common FPS displays the current frame rate directly in the game UI. The project
is open source, provides both a standalone ELF and an etaHEN plugin, follows
game/ShellUI lifecycle changes, and restores the overlay after supported
Rest Mode transitions.

## Features

- real-time integer FPS for PS4 and PS5 games;
- compact bottom-left overlay;
- purple `FPS:` label and white FPS value;
- smaller 24px default font in v1.2.0;
- automatic `FPS: Loading` state while the counter source is being prepared;
- preferred per-game VideoOut sampling;
- adaptive `/dev/dce` fallback when a VideoOut counter cannot be validated;
- no game-memory writes from the FPS sampler;
- automatic game PID reattachment;
- automatic ShellUI PID recovery;
- Rest Mode recovery on tested firmware;
- standalone ELF and persistent etaHEN plugin builds;
- source-built controller, plugin and ShellUI component;
- automated host tests, PS5 source build and artifact verification.

## Firmware status

| System software | Status | Notes |
|---|---|---|
| **4.51** | **Confirmed** | PS4 + PS5 games, 30/60 FPS, game switching, Rest Mode recovery and normal reboot tested |
| **9.60** | **Confirmed baseline** | Existing hardware-tested Common FPS lifecycle and FPS path |
| **7.60** | Test pending | Current v1.2.0 build needs a fresh hardware regression test |
| **10.xx** | Experimental | Code path is implemented, but current v1.2.0 still needs hardware validation |

A code path being present does not mean every firmware revision has been
physically tested. Confirmed and experimental status are intentionally kept
separate.

## Download

Use **one** of these two files:

```text
Common_FPS_PS5_v1.2.0.elf
Common_FPS_PS5_etaHEN_v1.2.0.plugin
```

The internal `Common_FPS_ShellUI_v1.2.0.elf` is embedded into the controller
and is not meant to be launched manually.

**Do not run the standalone ELF and etaHEN plugin at the same time.**

The latest release is available from the GitHub Releases page:

https://github.com/porhe911/Common-FPS-for-PS5/releases/latest

## Installation

### Standalone ELF

1. Start your jailbreak/etaHEN environment.
2. Send `Common_FPS_PS5_v1.2.0.elf` with your preferred payload launcher.
3. Start a PS4 or PS5 game.
4. The overlay appears automatically after the game and ShellUI readiness
   checks complete.

### etaHEN plugin

1. Install `Common_FPS_PS5_etaHEN_v1.2.0.plugin` through etaHEN's plugin
   system.
2. Enable the plugin/autoload option.
3. Start a game.
4. Common FPS waits for a stable game process before creating the overlay.

The plugin is the recommended option when you want the counter to come back
automatically after normal console lifecycle events.

## How FPS is measured

Common FPS prefers a validated VideoOut counter associated with the active
game. If that counter cannot be resolved safely, it can fall back to the PS5
display controller through `/dev/dce`.

The DCE path:

- does not depend on per-game hardcoded offsets;
- does not write to game memory;
- automatically searches the returned DCE data for a stable display-like
  counter when the fixed field is not suitable;
- was confirmed on FW 4.51 with both 30 FPS and 60 FPS games.

After DCE becomes the active source, expensive VideoOut discovery is placed in
a 60-second backoff window. If DCE stops producing valid samples, VideoOut
recovery is allowed immediately after repeated misses.

## Overlay

Default appearance in v1.2.0:

```text
Position: bottom-left
Font size: 24
FPS: label: purple
FPS value: white
Format: integer only
```

Example:

```text
FPS: 60
```

## Rest Mode and process recovery

The controller does not assume that the ShellUI or game PID stays constant.

When the game changes, the sampler resets and attaches to the new game
process. When ShellUI is recreated, Common FPS waits for the new PID to become
stable and restores the renderer.

FW 4.51 testing confirmed that FPS returns after Rest Mode and that a normal
system reboot still works correctly.

## Logs

Controller:

```text
/data/CommonFPS_v1_2_0.log
```

ShellUI:

```text
/data/CommonFPS_v1_2_0_shellui.log
```

If the overlay shows `FPS: Loading`, the controller log is the first file to
check.

## Build from source

The repository contains a reproducible PS5 build workflow.

GitHub Actions:

- **Host Source Tests**
- **PS5 Source Build**

The build produces:

```text
Common_FPS_PS5_v1.2.0.elf
Common_FPS_PS5_etaHEN_v1.2.0.plugin
Common_FPS_ShellUI_v1.2.0.elf
SHA256SUMS.txt
```

Local build details are in [BUILDING.md](BUILDING.md).

## Documentation

- [Full Russian documentation](docs/COMMON_FPS_FULL_DOCUMENTATION_RU.md)
- [v1.2.0 architecture and lifecycle](docs/V1_2_0_ARCHITECTURE_RU.md)
- [FW 4.51 hardware evidence](docs/evidence/STAGE8_8_FW451_HARDWARE_20260928.md)
- [v1.2.0 release notes](release/RELEASE_NOTES_v1.2.0.md)

## License

Common FPS-owned source is licensed under **GPL-3.0-or-later**. Third-party
projects keep their own licenses and notices.

Homebrew software for modified PlayStation 5 systems. Use at your own risk.
