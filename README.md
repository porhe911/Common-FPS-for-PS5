# Common FPS for PS5 — 1.1.0

This hardware-validated source snapshot combines the tracked-process lifecycle
with the source-built renderer/IPC path that produces the visible counter:

```text
Common_FPS_PS5_v1.1.0.elf
Common_FPS_PS5_etaHEN_v1.1.0.plugin
```

The controller runs in the process already spawned and recorded by etaHEN
instead of creating an untracked child with a second `fork()`. The sampler is
unchanged from the hardware-successful tracked-process run. v1.1.0 adds only the
embedded 70,968-byte source renderer and one-way state packets.

| Artifact | Expected SHA-256 |
|---|---|
| ELF | `4f544fa00f7a430e64c4c8d0ed42d0463d2370c81dabd9141599c27c4f3f99d6` |
| plugin | `39333081ecd93ade60d1b75fb0032a1e996fcf17ad47a9adfc0290591596e44e` |
| ShellUI renderer | `7880aec891cb95cc860753d5a3fed1dfbb23caf526b6106275c3b2cc02b8e465` |

## Scope

The v1.1.0 implementation:

- preserves the source-reproduced sampler and observer;
- removes only Common FPS's internal `fork()`;
- keeps the etaHEN-spawned PID resident and trackable;
- discovers `eboot.bin` through the FW 9.60 `KERN_PROC` layout;
- resolves `libSceVideoOut.sprx`;
- reads the VideoOut counter through the FW 9.60 DMAP page walk;
- calculates one-second integer FPS samples;
- follows game PID changes;
- writes one first-sample record for each game PID;
- embeds the exact 70,968-byte renderer used by the successful TEST21 visual
  run;
- injects it with the target-stack bootstrap and restores controller Auth-ID;
- sends one validated loopback state packet per second;
- creates and updates the integer FPS widget on ShellUI's update hook;
- contains no shutdown recorder or shutdown-time file writes;
- passed repeated PS5 runs with a visible counter in two games and normal
  system-menu restarts.

The submitted v1.1.0 log records `ShellUI renderer online`, `first_fps=59` and
`first_fps=60` for two game PIDs. The user also confirmed repeated normal
restarts without an improper-shutdown warning. The earlier sampler-only
baseline remains available in the repository history.

The visually complete `v1.0.0` release remains available as a fallback. This is
the promoted v1.1.0 source snapshot.

## Build

The easiest reproducible build is the GitHub Actions workflow **PS5 Source
Build**. It pins PS5 Payload SDK v0.41 and etaHEN source 2.4B
(`d47f99bd37f349ae59b3c4b66e09e93ba69f56cd`). The tested runtime was etaHEN
2.6; the pinned 2.4B tree is a build dependency.

For local build details, see [BUILDING.md](BUILDING.md).

## Evidence

The exact hardware procedure is in
[docs/V1_1_0_RELEASE.md](docs/V1_1_0_RELEASE.md).
The submitted run is retained at
[docs/evidence/V1_1_0_HARDWARE_20260905.log](docs/evidence/V1_1_0_HARDWARE_20260905.log)
(`sha256=7a2c1f27835e31026b663fdbe44e58a3d59e6675d5963073a626838ecb90a95c`).

## License

Common FPS-owned source is licensed under GPL-3.0-or-later. Third-party
projects retain their own licenses and notices.

Homebrew software for modified PlayStation 5 systems. Use at your own risk.
