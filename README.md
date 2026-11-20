# Common FPS for PS5 v1.2.5

Real-time integer FPS overlay for PS4 and PS5 games on a modified PlayStation 5.
Available as a standalone ELF or etaHEN plugin.

## Download and installation

Download **one** file from the [latest release](https://github.com/porhe911/Common-FPS-for-PS5/releases/latest):

- `Common_FPS_PS5_v1.2.5.elf` — launch with your usual payload loader.
- `Common_FPS_PS5_etaHEN_v1.2.5.plugin` — install and enable through etaHEN.

Reboot before replacing an older version, then start one ELF or plugin and
launch a game. The overlay appears after the game and ShellUI readiness checks.
**Do not run the ELF and plugin together.**

## Features and settings

- FPS display for supported PS4 and PS5 games using DCE flip counters or HEN-shared FPS samples. v1.2.5 disables VideoOut game-process memory scanning on all firmwares.
- Compact bottom-left overlay by default: 22-unit font, purple label, white digits.
- Startup settling and filtering of brief FPS spikes; displayed readings are capped at 120 FPS. A DCE/video-output reading is **not a measurement of uncapped game-engine rendering FPS**.
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
| **11.60** | v1.2.3 tested by the user on 2026-10-16; v1.2.5 **not yet tested** |
| **4.51** | Previously confirmed PS4/PS5 FPS, game switching and Rest Mode recovery; v1.2.5 **not yet tested** |
| **9.0 (user-reported; SDK 0x09050005 in logs)** | **v1.2.5 user-tested** on 2026-11-21 with GTA IV PS5 port, game's 120 FPS mode, 60 Hz TV; overlay and game reported working without the earlier crash |
| **9.60** | Previously confirmed FPS/lifecycle baseline; v1.2.5 **not yet tested** |
| **7.60** | Fresh hardware test pending |
| **10.xx** | Experimental; hardware validation pending |

**Scope of v1.2.5 confirmation:** the user tested the PS5 on firmware
reported as 9.0, with SDK `0x09050005` in its logs, and a **60 Hz TV**.
In GTA IV the game's selectable 120 FPS mode no longer crashed with the
v1.2.5 overlay active. **This does not verify real 120 Hz video output,
240 FPS, uncapped game-engine FPS, other firmware, or Rest Mode.** The
DCE-only sampling change applies to every firmware in source, but display
availability/accuracy is not proven on all of them. Firmware 11.60 was last
tested with v1.2.3 (2026-10-16); the prior baselines for 4.51 and 9.60
remain historical, not new v1.2.5 tests.
See the [v1.2.5 release notes](release/RELEASE_NOTES_v1.2.5.md).

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
