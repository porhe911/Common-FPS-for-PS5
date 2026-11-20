# Common FPS for PS5 v1.2.5

## Changes since v1.2.4

- **Fixed the GTA IV 120 FPS-mode crash observed with Common FPS on the
  user's PS5 firmware 9.0.** The v1.2.5 DCE-only test build was reported
  working with the FPS overlay enabled and GTA IV's selectable 120 FPS
  mode; the crash did not recur.
- The controller no longer scans or reads **game-process memory** for
  the VideoOut frame counter. This safe mode is compiled in for every
  firmware; FPS comes from DCE flip counters or etaHEN shared samples
  when available. ShellUI initialization still accesses its renderer
  target in the existing controlled manner.
- Preserved the Media → Common FPS settings shortcut, 4-corner position,
  font size, colors, saved config, autoload lifecycle and diagnostic logs
  in `/data/CommonFPS/`.
- Preserved the **120 FPS display ceiling**. This is an FPS **display**
  limit, not an in-game frame limiter. DCE output-flip sampling does not
  guarantee measuring internal game-engine FPS above display refresh.

## Installation

Download **only one** of the two assets:

- `Common_FPS_PS5_v1.2.5.elf` — standalone payload.
- `Common_FPS_PS5_etaHEN_v1.2.5.plugin` — etaHEN plugin.

Fully reboot before updating from v1.2.4, to unload the resident
controller and ShellUI renderer. Disable the old version's etaHEN autoload
before enabling the new plugin. **Never load ELF and plugin together.**
Settings persist in `/data/CommonFPS/config.ini`.

## Verified scope and limitations

**Hardware runtime PASS (user report, 2026-11-21):** PS5 system firmware
described as **9.0** (diagnostic SDK was `0x09050005`), 60 Hz TV, GTA IV
PS5 port, game option **120 FPS**, active v1.2.5 overlay, no repeat crash.

The 60 Hz TV cannot validate real **120 Hz output** or displayed 120 FPS.
No 120 Hz or 240 Hz TV, alternative firmware, or uncapped FPS comparison
was available. These cases and other games require community testing.
The source-level no-game-memory-read change is global across firmware,
but FPS availability and accuracy from DCE/HEN are not validated for every
firmware. No universal hardware compatibility guarantee is made.

GitHub Actions source build, host regression tests and release artifact
verification must pass before publication. The earlier v1.2.4 release
and historical tags are preserved.

## Logs

- Controller: `/data/CommonFPS/CommonFPS.log`
- ShellUI renderer: `/data/CommonFPS/CommonFPS_shellui.log`

Logs reset on each activation and are limited to 64 KiB each.
GPL-3.0-or-later; see LICENSE and THIRD_PARTY_NOTICES.md.
