# Common FPS for PS5 v1.2.5 — DCE-only safety A/B candidate

**Status: diagnostic test build, NOT a stable release.** Published v1.2.4
remains unchanged. Do not merge or publish without successful hardware QA.

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

## Required test

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

**Offline/structural PASS is not PS5 runtime PASS.**
