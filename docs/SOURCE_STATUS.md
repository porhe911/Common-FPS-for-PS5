# Source status — TEST31

This branch is the source snapshot for PARITY TEST31.

The reference Release build produced:

```text
360f9103d552b1c812febe4bcfc16647cf5497475592154d1273fdb379a6cfea  ELF
b68f2242434773c2b65393b0f06b4b1db926e14e8f5d51b2e8b2e358d3e1832b  plugin
```

The snapshot is based on the hardware-validated TEST30 source. Its controller
still has no internal `fork()` and keeps the sampler in etaHEN's tracked PID;
TEST31 adds the exact 70,968-byte source renderer, target-stack bootstrap and
loopback packet sender needed for the on-screen value.

TEST30 already passed a two-game hardware run and normal restart. TEST31 is a
diagnostic candidate until its combined visible-FPS/restart gate passes.

See [PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md](PARITY_TEST31_TRACKED_RENDERER_NO_FORK_AB.md).
