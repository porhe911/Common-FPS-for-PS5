# Common FPS for PS5 v1.2.5 — DCE-only safety A/B record

**Status: test build user-confirmed on reported FW 9.0 and promoted for
stable v1.2.5 publication by explicit user request (2026-11-21).** This is
a narrow hardware PASS; other firmware and actual 120 Hz output are untested.

## Why this candidate exists

With v1.2.4 enabled, the user reported a GTA IV native PS5 port crash
while testing the game's 120 FPS option. After fully rebooting the console
and running the same GTA IV build at 120 FPS *without the Common FPS
autoload enabled*, the user reported no crash. This A/B strongly increases
suspicion of interaction but is not definitive proof of the mechanism.

Earlier klog recorded a SIGSEGV in the game's ReXGlue worker called
`Kernel Host Tasks`; Common FPS logs showed successful ShellUI setup and
repeated VideoOut process-memory discovery, but reached their 64 KiB cap.
There is not yet evidence proving that the game reads caused the crash.

## Changes relative to v1.2.4

- Build-wide (all firmware): compile-time false guard around all
  `FpsSampler::attach()` and `FpsSampler::sample()` calls. The controller
  will not discover, scan or read the active game's VideoOut memory.
- Retain DCE flip-counter FPS, shared HEN FPS when available, game PID
  read-only enumeration, ShellUI renderer, user settings, palettes,
  Media shortcut, filtering and normal IPC behavior.
- ShellUI bootstrap still needs its own established code read/write
  sequence. **This build does not disable all process access**; it
  isolates game-process memory sampling only.
- Diagnostics contain `SAFETY_DIAGNOSTIC=1 game_memory_reads=0
  game_memory_writes=0 VideoOut_scan=disabled`, and the v1.2.5 version.
- No change to the game's framerate, timing, engine, or files.
- Source build and host tests verify the DCE-only gate, version and plugin.

## Completed user test (2026-11-21)

- PS5 firmware as reported by user: **9.0**. Previous logs exposed SDK
  `0x09050005`; these are distinct labels and are preserved as reported.
- Screen refresh: **60 Hz**. Game mode: user-selected **120 FPS** in GTA IV.
- With v1.2.4: SIGSEGV had occurred in the GTA IV native PS5 port.
- With v1.2.5 test ELF/plugin: user reported **excellent working state**,
  overlay present and crash not reproduced.
- No measured video output at 120 Hz, FPS above 120, multi-console coverage,
  alternate firmware, Rest Mode or game-switch regression was provided.
- These observations support the DCE-only sampler choice but do not prove
  VideoOut scanning as the sole underlying cause.

## Follow-up testing for other users (not release blockers)

1. Confirm the prior no-plugin **GTA IV at 120 FPS** test was performed
   after reboot with etaHEN Common FPS autoload **disabled**.
2. Reboot PS5, and enable **exactly one** new v1.2.5 test ELF or plugin.
   Never mix versions or load ELF and plugin together.
3. Open GTA IV, select 120 FPS, reproduce the same area/scene and play
   beyond the previously observed crash point. Observe both FPS overlay
   and game stability. If it crashes, record the exact point.
4. If stable, test GTA IV at 60 FPS, then one other PS4 and one PS5 game.
   Repeat 120 FPS on a separate supported game if available.
5. Collect `/data/CommonFPS/CommonFPS.log`,
   `/data/CommonFPS/CommonFPS_shellui.log` and klog after the test.

Expected: overlay may use `dce-fixed`, `dce-adaptive` or `hen-shared`.
No `Sampler indirect scan`, `Sampler read backend`,
`Sampler module`, or `videoout-process` should appear in controller
log. A missing FPS number on a firmware lacking DCE/HEN support must be
reported, not masked as a pass.

## Interpretation

- Stable 120 FPS **with v1.2.5**: supports a causal role for the old
  game-process VideoOut scanning; still not universal firmware validation.
- Crash still occurs: investigate ShellUI hook/injection, DCE ioctl,
  scheduling/interactions and GTA port; do not publish as fixed.
- Stable but FPS missing/inaccurate: investigate DCE/HEN support and
  sampling accuracy; a universal release needs a separate safe source.

**Offline/structural PASS and this single-game runtime PASS are separate.**
This release does not claim universal firmware coverage or unrestricted FPS.
