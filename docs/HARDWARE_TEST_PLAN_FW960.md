# FW 9.60 hardware test plan

The hardware-stable historical v1.0.0 remains the behavioral baseline.

## Stable-v1 reconstruction order

1. Reproduce the historical sampler and process-memory read path.
2. Validate it independently with a bounded diagnostic ELF.
3. Reconstruct the historical renderer/loader contract.
4. Validate manual-run parity.
5. Only then add autoload readiness around the proven initialization.

## Parity probe v2

`Common_FPS_PS5_v1_parity_probe_FW960.elf` is a read-only diagnostic target. It installs no ShellUI renderer, no hook, and no autoload behavior.

Run it manually while a game is already active. Every completed stage is flushed to:

`/data/CommonFPS_v1_probe.log`

Stages:

- `S0` diagnostic `main()` entered
- `S1` `eboot.bin` process found
- `S2` `libSceVideoOut.sprx` found
- `S3` PHU-style FW 9.60 DMAP table read completed
- `S4` stable sampler resolved the VideoOut counter
- `S5` real FPS sample obtained

If screen notifications are unavailable, the file log remains authoritative. If the file is not created at all, investigate the standalone ELF launch/entry-point path before changing the sampler.

Do not install this probe as an etaHEN plugin and do not put it in Autoload.
