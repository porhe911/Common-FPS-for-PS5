# Source status — TEST31

This branch is the source snapshot for PARITY TEST31.

The reference Release build produced:

```text
2db81cb2f896eb5310e22715226cc065e1b2c22304f6376c0fdbac5ce32b9f3a  ELF
fefdab4c49fc58eddfd798697d1479818367db67d6543f1994809d3002fcbc19  plugin
```

The snapshot is based on the hardware-validated TEST30 source. Its controller
still has no internal `fork()` and keeps the sampler in etaHEN's tracked PID;
TEST31 adds the exact 70,968-byte source renderer, target-stack bootstrap and
loopback packet sender needed for the on-screen value.

TEST30 already passed a two-game hardware run and normal restart. TEST31 is a
diagnostic candidate until its combined visible-FPS/restart gate passes.

See [PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md](PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md).
