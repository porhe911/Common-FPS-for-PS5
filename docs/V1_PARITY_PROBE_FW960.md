# Stable v1 parity probe — FW 9.60

`Common_FPS_PS5_v1_parity_probe_FW960.elf` is a manual diagnostic payload for the reconstructed hardware-stable v1.0.0 FPS sampler.

It is deliberately separate from the shipping controller/plugin and does not install the ShellUI renderer. The probe uses:

- `V1StableController`
- `V1StableSampler`
- `V1StablePs5Platform`
- PHU-style DMAP translation/read path for FW 9.60

## Hardware test

1. Boot etaHEN normally.
2. Start a game and wait until gameplay is visible.
3. Inject `Common_FPS_PS5_v1_parity_probe_FW960.elf` manually.
4. Do not enable this ELF as an etaHEN autoload plugin.
5. Wait for one notification.

Expected success notification:

`Common FPS parity probe` followed by `DMAP FW 9.60 OK: <value> FPS`.

The ELF exits after the first numeric FPS sample. If no valid sample is recovered within 90 one-second lifecycle iterations, it emits `No valid FPS after 90 seconds` and exits with status 2.

A successful hardware result validates the reconstructed game-process/module discovery, VideoOut counter resolution, PHU-style page-table translation and physical DMAP reads before any renderer/autoload integration is attempted.
