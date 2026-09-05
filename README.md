# Common FPS for PS5 — 1.1.0

This hardware-validated source snapshot combines TEST30's tracked-process
lifecycle with the source-built renderer/IPC path that produces the visible
counter:

```text
Common_FPS_PS5_v1.1.0_PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.elf
Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.plugin
```

The controller runs in the process already spawned and recorded by etaHEN
instead of creating an untracked child with a second `fork()`. The sampler is
unchanged from the hardware-successful TEST30 run. TEST31 adds only the
embedded 70,968-byte source renderer and one-way state packets.

| Artifact | Expected SHA-256 |
|---|---|
| ELF | `2db81cb2f896eb5310e22715226cc065e1b2c22304f6376c0fdbac5ce32b9f3a` |
| plugin | `fefdab4c49fc58eddfd798697d1479818367db67d6543f1994809d3002fcbc19` |

## Scope

The TEST31 implementation:

- preserves the source-reproduced TEST20 sampler and observer;
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

The submitted TEST31 log records `ShellUI renderer online`, `first_fps=59` and
`first_fps=60` for two game PIDs. The user also confirmed repeated normal
restarts without an improper-shutdown warning. The source-identical TEST20
baseline remains on branch `parity-test20-source`.

The visually complete `v1.0.0` release remains available as a fallback. TEST31
is the promoted v1.1.0 source snapshot.

## Build

The easiest reproducible build is the GitHub Actions workflow **PS5 Source
Build**. It pins PS5 Payload SDK v0.41 and etaHEN source 2.4B
(`d47f99bd37f349ae59b3c4b66e09e93ba69f56cd`). The tested runtime was etaHEN
2.6; the pinned 2.4B tree is a build dependency.

For local build details, see [BUILDING.md](BUILDING.md).

## Evidence

The exact hardware procedure is in
[docs/PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md](docs/PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md).
The submitted run is retained at
[docs/evidence/PARITY_TEST31_HARDWARE_20260905.log](docs/evidence/PARITY_TEST31_HARDWARE_20260905.log)
(`sha256=7a2c1f27835e31026b663fdbe44e58a3d59e6675d5963073a626838ecb90a95c`).

## License

Common FPS-owned source is licensed under GPL-3.0-or-later. Third-party
projects retain their own licenses and notices.

Homebrew software for modified PlayStation 5 systems. Use at your own risk.
