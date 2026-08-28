# Source status

## v1.1.0-rc1

This repository is the clean source-built successor branch.

The source-built path contains no historical PHU renderer ELF/SO blob.

Common FPS-owned core, lifecycle, configuration, sampler and Safe Autoload
logic are published directly as source.

PS5-specific integration is built against pinned/open GPL upstream source.

## Hardware-stable baseline

The existing public `v1.0.0` binary remains the hardware-stable baseline for
manual activation on FW 9.60.

Its historical binary was not originally produced from this clean source tree,
so this repository does not pretend otherwise.

## Promotion to v1.1.0 stable

The RC becomes stable only when:

1. GitHub `Host Source Tests` passes;
2. GitHub `PS5 Source Build` passes;
3. source-built ELF/plugin artifacts are produced;
4. FW 9.60 manual-start testing passes;
5. FW 9.60 etaHEN autoload testing passes;
6. repeated game-switch lifecycle testing passes without KP.

Until then the correct public version label is `v1.1.0-rc1`.

## Safe Autoload integration status

The production PS5 worker now uses `AutoloadGuard` through the real
`Ps5AutoloadBackend`.

Stage A implements:

- stable `SceShellUI` + Mono readiness gating;
- source companion loading with `elfldr_debug()`;
- one continuous ptrace loader session;
- no `elfldr_exec()` / no intentional `SIGKILL` failure path;
- receiver heartbeat and PID-aware health checking;
- duplicate/retry throttling;
- an additional `VisualReady` gate before any game lifecycle activity;
- build-time embedding of the source-built ShellUI companion into the main
  controller ELF.

End-user deployment is therefore single-file:

- etaHEN users install only `Common_FPS_PS5_etaHEN_v1.1.0.plugin` in
  `/data/etaHEN/plugins/`;
- standalone / YouTube Jailbreak autoload users use only
  `Common_FPS_PS5_v1.1.0.elf` in their normal ELF/autoload location;
- `/data/CommonFPS/` is not required to carry a renderer file.

The separate `Common_FPS_ShellUI_v1.1.0.elf` remains in source-build artifacts
only for diagnostics and source transparency.

Stage A intentionally does **not** emit `VisualReady`. The source-built visual
renderer still needs the Stage B Mono/PUI main-thread bootstrap. Until that is
implemented and tested, Stage A is a safety probe rather than a functional FPS
release.

See `docs/AUTOLOAD_STAGE_A_HW_PROBE.md`.
