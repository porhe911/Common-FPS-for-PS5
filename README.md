# Common FPS for PS5 v1.2.1

Real-time FPS overlay for PS4 and PS5 games on a modified PlayStation 5.

Common FPS displays the current frame rate directly over the game and is
available as a standalone ELF or etaHEN plugin.

## Features

- real-time integer FPS for PS4 and PS5 games;
- compact bottom-left overlay;
- purple `FPS:` label and white FPS value;
- 24px default font;
- `FPS: Loading` while the FPS source is being prepared;
- VideoOut sampling with adaptive `/dev/dce` fallback;
- no game-memory writes from the FPS sampler;
- automatic game PID reattachment;
- automatic ShellUI PID recovery;
- Rest Mode recovery on tested firmware;
- standalone ELF and etaHEN plugin builds;
- automated source build and artifact verification.

## Firmware status

| System software | Status | Notes |
|---|---|---|
| **4.51** | **Confirmed** | PS4 + PS5 games, 30/60 FPS, game switching, Rest Mode recovery, normal reboot |
| **9.00** | **Confirmed** | Shadow of the Colossus and Spider-Man 2 tested; FPS survives Rest Mode |
| **9.60** | **Confirmed baseline** | Existing hardware-tested FPS and lifecycle path |
| **7.60** | Test pending | Current v1.2.1 build needs a fresh hardware regression test |
| **10.xx** | Experimental | Code path exists, but hardware validation is still required |

A code path being present does not mean every firmware revision has been
physically tested. Confirmed and experimental status are kept separate.

## Download

Use **one** of these two files from the latest GitHub Release:

```text
Common_FPS_PS5_v1.2.1.elf
Common_FPS_PS5_etaHEN_v1.2.1.plugin
```

**Do not run the standalone ELF and etaHEN plugin at the same time.**

Latest release:

https://github.com/porhe911/Common-FPS-for-PS5/releases/latest

## Installation

### Standalone ELF

1. Start your jailbreak/etaHEN environment.
2. Send `Common_FPS_PS5_v1.2.1.elf` with your preferred payload launcher.
3. Start a PS4 or PS5 game.
4. The overlay appears automatically after the game and ShellUI readiness
   checks complete.

### etaHEN plugin

1. Install `Common_FPS_PS5_etaHEN_v1.2.1.plugin` through etaHEN's plugin
   system.
2. Enable the plugin/autoload option.
3. Start a game.
4. Common FPS waits for a stable game process before creating the overlay.

The plugin is the recommended option for persistent use.

## What's new in v1.2.1

FW 9.00 required a different ShellUI write path than the one used by earlier
tested firmware. Reading the target JIT code through MDBG worked, but the
verified one-byte UI-thread guard could not be written while the page remained
RX.

v1.2.1 adds a FW 9.00-specific safe transaction:

- ShellUI is stopped;
- expected bytes are verified;
- the target 16 KiB JIT page is temporarily changed from RX to RWX;
- only the required one-byte guard is written;
- the page is restored to RX immediately;
- the result is read back and verified;
- auth state is restored before the transaction completes.

No game-process write path was added.

## How FPS is measured

Common FPS prefers a validated VideoOut counter associated with the active
game. If that counter cannot be resolved safely, it falls back to the PS5
display controller through `/dev/dce`.

The DCE path:

- does not depend on per-game hardcoded offsets;
- does not write to game memory;
- can automatically identify a stable display-like counter;
- is confirmed on FW 4.51 and FW 9.00.

When DCE is healthy, expensive VideoOut discovery uses a 60-second backoff.
If DCE stops producing valid samples, VideoOut recovery is allowed again.

## Overlay

Default appearance:

```text
Position: bottom-left
Font size: 24
FPS: label: purple
FPS value: white
Format: integer only
```

## Rest Mode and recovery

Common FPS tracks both the active game process and `SceShellUI`.

When a game or ShellUI PID changes, the old state is discarded and the
required sampler/renderer components are brought up again.

Rest Mode recovery is hardware-confirmed on FW 4.51 and FW 9.00.

## Logs

Controller:

```text
/data/CommonFPS_v1_2_1.log
```

ShellUI:

```text
/data/CommonFPS_v1_2_1_shellui.log
```

If the overlay shows `FPS: Loading`, the controller log is the first file to
check.

## Build from source

GitHub Actions runs:

- **Host Source Tests**
- **PS5 Source Build**

The PS5 build produces:

```text
Common_FPS_PS5_v1.2.1.elf
Common_FPS_PS5_etaHEN_v1.2.1.plugin
Common_FPS_ShellUI_v1.2.1.elf
SHA256SUMS.txt
```

## License

Common FPS-owned source is licensed under **GPL-3.0-or-later**.

Homebrew software for modified PlayStation 5 systems. Use at your own risk.
