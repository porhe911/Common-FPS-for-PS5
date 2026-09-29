# Common FPS for PS5 v1.2.1

## Changes

- Added confirmed FW 9.00 support.
- Fixed the ShellUI one-byte guard transaction on FW 9.00.
- FW 9.00 now uses MDBG for the verified ShellUI probe/guard path.
- The target JIT page is temporarily unlocked only for the verified one-byte
  write, then restored to RX before readback verification.
- No game-process write path was added.
- Rest Mode recovery is confirmed on FW 9.00.
- Shadow of the Colossus and Spider-Man 2 were tested successfully on FW 9.00.
- Existing FW 4.51 and FW 9.60 paths remain unchanged.
- Overlay remains compact with the 24px default font.

## Firmware status

| System software | Status |
|---|---|
| 4.51 | Confirmed |
| 9.00 | Confirmed |
| 9.60 | Confirmed baseline |
| 7.60 | Fresh v1.2.1 test pending |
| 10.xx | Experimental / hardware test pending |

## Files

Standalone:

`Common_FPS_PS5_v1.2.1.elf`

etaHEN plugin:

`Common_FPS_PS5_etaHEN_v1.2.1.plugin`

**Do not run ELF and plugin at the same time.**

## License

GPL-3.0-or-later.
