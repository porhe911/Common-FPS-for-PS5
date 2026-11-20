# Common FPS for PS5 v1.2.4

## Changes since v1.2.3

- Fixed the missing FPS overlay on the affected PS5 firmware 9.x console
  (reported as system software 9.0; firmware SDK `0x09050005`). The previous
  build obtained FPS but selected an incompatible ShellUI `ptrace-io`
  probe (EFAULT), then quarantined the renderer. The observed SDK family now
  uses the earlier FW 9.00 MDBG probe and stopped one-byte guard path.
- Moved both diagnostic logs to the existing settings directory:
  `/data/CommonFPS/CommonFPS.log` and
  `/data/CommonFPS/CommonFPS_shellui.log`.
  The directory is created before initializing the session logs.
- Preserved reset-on-activation logs limited to 64 KiB each; the offline
  Media → Common FPS settings shortcut, colors, corners, size and margins;
  existing `/data/CommonFPS/config.ini`; and FPS sampling/filtering.
- Added regression tests for SDK 9.05 routing, checked MDBG reads and new
  log locations. Existing 9.60 and 11.60 backend routing is unchanged.

## Installation

Download **one** file:

- `Common_FPS_PS5_v1.2.4.elf` — standalone payload.
- `Common_FPS_PS5_etaHEN_v1.2.4.plugin` — etaHEN plugin.

**Reboot the console before updating** to unload an older resident
renderer. Do not run the ELF and plugin simultaneously. Existing saved
settings remain intact.

Open **Media → Common FPS** to adjust the location, appearance and
colors. D-pad / ✕ selects controls; OPTIONS saves settings.

## Verification and firmware scope

On 2026-11-20, the user reported that the v1.2.4 test version now displays
FPS and works on the previously affected FW 9.x console. The console was
described as running firmware 9.0 and the collected logs reported SDK
`0x09050005`. This confirms **that FPS-display workflow**; it is not a
fresh exhaustive check of all games, resolutions, game switching or Rest Mode.

FW 11.60 was successfully verified with v1.2.3 on 2026-10-16; v1.2.4
has not been separately tested on FW 11.60 hardware. FW 4.51, 9.00 and
9.60 have previous working baselines, pending fresh v1.2.4 regression
tests. FW 7.60 needs testing and 10.xx remains experimental. The
Media installer is skipped on FW 12+.

PS5 Source Build, Host Source Tests, FW 11.60 Preflight Build and the
source artifact verifier passed for v1.2.4.

## Logs and license

The v1.2.4 log files reside in `/data/CommonFPS/`, not the root of
`/data`. Old root-level v1.2.3 logs are not removed automatically.
Licensed under GPL-3.0-or-later; see the third-party notices.
