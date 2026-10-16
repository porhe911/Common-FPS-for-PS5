# Common FPS for PS5 v1.2.3

Stable promotion of RC9 after the user's FW 11.60 confirmation.

## Changes since v1.2.1

- Working FPS overlay on FW 11.60, with checked ShellUI code access and deferred
  renderer startup.
- Compact default overlay: 22-unit font and lower bottom-left placement.
- Startup settling and filtering of brief FPS spikes while keeping sustained
  120 FPS support.
- Offline **Media → Common FPS** settings shortcut, installed when missing
  on firmware below 12.
- Four corners, adjustable size/margins, optional FPS label and separate colors
  for label and numbers. Settings persist across restarts and apply live.
- Explicit eight-color palettes plus custom HEX colors.
- Reliable D-pad/**✕** activation, duplicate-event suppression and **OPTIONS**
  saving. Opening a palette does not change the selected color.
- Improved renderer object lifetimes, selective style updates and recovery
  when the ShellUI root widget changes.
- Controller and renderer logs reset on each activation and are limited to
  **64 KiB each**.

The stable build changes RC9's version labels and release metadata only;
sampling, settings behavior, rendering and injection logic are preserved.

## Installation

Choose **one** asset:

- `Common_FPS_PS5_v1.2.3.elf` — standalone payload.
- `Common_FPS_PS5_etaHEN_v1.2.3.plugin` — etaHEN plugin.

**Reboot before upgrading** to unload an older resident renderer. Use your
usual working loader, or enable the plugin through etaHEN. Do not run both.
Existing settings in `/data/CommonFPS/config.ini` are retained.

Open **Media → Common FPS**, choose your settings and press **✕** on **Save**,
or press **OPTIONS**. Changes apply on the next one-second update.

## Validation

FW 11.60: the user reported RC9 working on 2026-10-16 and authorized stable
publication. This confirms that tested workflow; every game, display mode and
Rest Mode scenario has not been newly verified. The precise cause of the
previous RC8 freeze was not established.

Previously confirmed FPS/lifecycle paths for FW 4.51, 9.00 and 9.60 are retained;
fresh v1.2.3 hardware regression tests are pending. FW 7.60 needs testing and
10.xx remains experimental. The Media installer is skipped on FW 12+.

Automated checks cover host behavior, controller interaction with the real
settings HTTP worker, source-built PS5 artifacts and the plugin wrapper.

## Logs and license

`/data/CommonFPS.log` and `/data/CommonFPS_shellui.log` are overwritten on each
activation, up to 64 KiB each. Settings are stored separately.

Common FPS-owned source: GPL-3.0-or-later.
