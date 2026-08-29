# Source status

## Hardware-stable baseline

The public `v1.0.0` binary remains the hardware-stable baseline for manual
activation on FW 9.60.

The released etaHEN plugin is the historical internal v0.28b build:

```text
plugin SHA-256  f1240511bfe3cb17a3db6f4d099cf3cd69be678d10901425a7b33911265195e1
payload SHA-256 6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c
wrapper          CFPS00020 / 0.28
```

Its historical binary was not originally produced from the current clean
source tree, so this repository does not pretend otherwise.

See `docs/V1_0_0_BINARY_FORENSICS.md` for the recovered module/symbol map and
embedded-renderer evidence.

## Current source reconstruction line

The active development direction is now **stable-architecture source
reconstruction**.

The goal is to publish maintainable source that reproduces the real v1.0.0
runtime contract before adding any new behavior:

- async `fork()` entry;
- stable SceShellUI wait/inject path;
- Trace-Continue loader session;
- full `elfldr_payload_args()` side effects;
- Scene readiness;
- read-only PS4/PS5 VideoOut FPS sampling;
- game-switch lifecycle;
- the proven bottom-left renderer behavior.

The historical stable ELF is not stripped and exposes enough symbol/source-unit
information to guide this reconstruction. The historical binary itself is not
required in the source tree; `tools/analyze_v1_stable.py` performs local
read-only verification when a user supplies a copy.

## Rejected source-renderer experiments

The Stage B, B2 and B3 branches are hardware rejected and must not be used as
parity targets. They attempted to introduce/chainsaw another live ShellUI
`Application.Update` hook and caused regressions including missing Common FPS,
Stop/Run kernel panic and broader etaHEN/YBJB instability.

The reconstruction must not repeat that design.

## Autoload policy

Autoload is still a release goal, but it is no longer allowed to drive a
renderer redesign.

The correct order is:

1. source-built manual-run build reaches v1.0.0 hardware parity;
2. PS4 + PS5 and Game A -> Home -> Game B pass without KP;
3. only then add a conservative readiness gate **before the same proven stable
   initialization**;
4. validate etaHEN Autoload separately;
5. investigate FW 7.60 `FPS: loading` as a separate compatibility issue.

## Promotion to a new stable source release

A new source release becomes stable only when:

1. GitHub `Host Source Tests` passes;
2. GitHub `PS5 Source Build` passes;
3. source-built ELF/plugin artifacts are produced without historical binary
   blobs being passed off as source;
4. FW 9.60 manual-start behavior matches v1.0.0;
5. PS4 and PS5 real FPS are verified;
6. repeated game-switch lifecycle testing passes without KP;
7. etaHEN Autoload passes without manual Stop/Run;
8. source/license/third-party notices accurately describe every reused upstream
   component.

Until those conditions are met, the historical public `v1.0.0` remains the
stable hardware release.
