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
