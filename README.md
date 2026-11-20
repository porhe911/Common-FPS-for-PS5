# Common FPS for PS5 v1.2.4

Real-time integer FPS overlay for PS4 and PS5 games on a modified PlayStation 5.
Available as a standalone ELF or etaHEN plugin.

## Download and installation

Download **one** file from the [latest release](https://github.com/porhe911/Common-FPS-for-PS5/releases/latest):

- `Common_FPS_PS5_v1.2.4.elf` — launch with your usual payload loader.
- `Common_FPS_PS5_etaHEN_v1.2.4.plugin` — install and enable through etaHEN.

Reboot before replacing an older version, then start one ELF or plugin and
launch a game. The overlay appears after the game and ShellUI readiness checks.
**Do not run the ELF and plugin together.**

## Features and settings

- FPS for PS4 and PS5 games, with VideoOut sampling and an adaptive DCE fallback.
- Compact bottom-left overlay by default: 22-unit font, purple label, white digits.
- Startup settling and filtering of brief FPS spikes; sustained 120 FPS is supported.
- Automatic game-process and ShellUI recovery on supported firmware.
- Offline **Media → Common FPS** settings shortcut, checked on activation and
  installed when missing (firmware below 12).
- Four corners, font size 14–64, adjustable edge margins, digits-only mode and
  separate colors for the label and numbers.
- Eight named colors in each palette, plus custom HEX colors.
- D-pad navigation, **✕** activation and **OPTIONS** to save.

Choose your settings, select **Save** and press **✕**, or press **OPTIONS**.
Saved changes apply on the next one-second update and persist across restarts
in `/data/CommonFPS/config.ini`.

The settings page is embedded and works offline while the counter is running.
Position and size use the PS5 interface's logical 1920×1080 canvas. Adjust font
size for viewing distance and margins for TV overscan; physical screen diagonal
is not queried. Other PUI canvas sizes still need hardware testing.

## Firmware status

| System software | Hardware validation |
|---|---|
| **11.60** | v1.2.3 tested by the user; working overlay and settings confirmed on 2026-10-16 |
| **4.51** | Previously confirmed PS4/PS5 FPS, game switching and Rest Mode recovery; v1.2.3 regression pending |
| **9.x (reported 9.0; SDK 0x09050005)** | v1.2.4 FPS overlay confirmed working on 2026-11-20; other scenarios not newly tested |
| **9.60** | Previously confirmed FPS/lifecycle baseline; v1.2.3 regression pending |
| **7.60** | Fresh hardware test pending |
| **10.xx** | Experimental; hardware validation pending |

The v1.2.4 confirmation covers the reported FW 9.x FPS display issue, not
every game, display mode or Rest Mode scenario. FW 11.60 was last hardware
validated with v1.2.3 and is not separately regression-tested on v1.2.4.
See the [v1.2.4 release notes](release/RELEASE_NOTES_v1.2.4.md).

## Logs

- Controller: `/data/CommonFPS/CommonFPS.log`
- Renderer: `/data/CommonFPS/CommonFPS_shellui.log`

The old v1.2.3 root-level logs may remain; they are not automatically deleted.

Both logs are overwritten on each activation and limited to **64 KiB each**.
Saved settings are kept separately.

## Source and license

See [BUILDING.md](BUILDING.md) for source builds and tests.
Common FPS-owned source is licensed under **GPL-3.0-or-later**;
see [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).
