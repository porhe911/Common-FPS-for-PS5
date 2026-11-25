## What's New in v1.2.6

- Added support for **PS5 firmware 13.0**, restoring the FPS overlay on the tested console.
- Fixed ShellUI initialization on firmware 13.0 using verified code reads and a protected guard transaction with restoration of the original memory protection.
- Preserved DCE/etaHEN FPS sampling without scanning or reading game-process memory, including the stability improvement introduced in v1.2.5.
- Retained the existing settings, overlay customization, and firmware-specific rendering paths.

## Installation

Download either the standalone ELF or the etaHEN plugin. Fully reboot the console before replacing an older version. Do not load both at the same time.

## Compatibility

**Confirmed on firmware 13.0:** FPS overlay working, renderer initialized successfully, 60 FPS readings and sampling recovery after changing game process. One transient DCE error was observed during the transition and sampling resumed.

Other firmware versions, 120 Hz output, Rest Mode and plugin autoload have not been retested with v1.2.6. The Media settings shortcut installer remains unavailable on firmware 12 and above. FPS display remains capped at 120; it does not change the game's FPS limit.

Logs: `/data/CommonFPS/CommonFPS.log` and `/data/CommonFPS/CommonFPS_shellui.log`.
