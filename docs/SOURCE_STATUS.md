# Source status — v1.1.0 / TEST31

This snapshot is the source for the hardware-validated v1.1.0 release.

The reference Release build produced:

```text
2db81cb2f896eb5310e22715226cc065e1b2c22304f6376c0fdbac5ce32b9f3a  ELF
fefdab4c49fc58eddfd798697d1479818367db67d6543f1994809d3002fcbc19  plugin
```

The snapshot is based on the hardware-validated TEST30 source. Its controller
still has no internal `fork()` and keeps the sampler in etaHEN's tracked PID;
TEST31 adds the exact 70,968-byte source renderer, target-stack bootstrap and
loopback packet sender needed for the on-screen value.

TEST30 passed the tracked-process boundary with two games and a normal restart.
TEST31 then passed the combined gate: the on-screen counter was visible in two
games (`59` and `60` FPS), and repeated normal system-menu restarts completed
without an improper-shutdown warning. The retained evidence log is
`docs/evidence/PARITY_TEST31_HARDWARE_20260905.log` with SHA-256
`7a2c1f27835e31026b663fdbe44e58a3d59e6675d5963073a626838ecb90a95c`.

See [PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md](PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md).
