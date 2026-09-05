# Source status — v1.1.0

This snapshot is the source for the hardware-validated v1.1.0 release.

The reference Release build produced:

```text
4f544fa00f7a430e64c4c8d0ed42d0463d2370c81dabd9141599c27c4f3f99d6  ELF
39333081ecd93ade60d1b75fb0032a1e996fcf17ad47a9adfc0290591596e44e  plugin
```

The snapshot keeps the hardware-validated tracked-process boundary. Its
controller has no internal `fork()` and keeps the sampler in etaHEN's tracked
PID; v1.1.0 adds the exact 70,968-byte source renderer, target-stack bootstrap
and loopback packet sender needed for the on-screen value.

The combined hardware gate passed: the on-screen counter was visible in two
games (`59` and `60` FPS), and repeated normal system-menu restarts completed
without an improper-shutdown warning. The retained evidence log is
`docs/evidence/V1_1_0_HARDWARE_20260905.log` with SHA-256
`7a2c1f27835e31026b663fdbe44e58a3d59e6675d5963073a626838ecb90a95c`.

See [V1_1_0_RELEASE.md](V1_1_0_RELEASE.md).
