# Common FPS for PS5 — PARITY TEST20 source

This branch contains the byte-reproducible source for:

```text
Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf
Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.plugin
```

The rebuilt files are byte-for-byte identical to the artifacts tested on a
PS5 with FW 9.60 and etaHEN 2.6.

| Artifact | Expected SHA-256 |
|---|---|
| ELF | `b791baf6f85063d0c7ec57eebcbf60116dff58dfc4ec74aa6d9dedcc1ecc32ef` |
| plugin | `7bdcc16cc582bd89c7f1680471436ef58fdc53bab159c7e36278a5e8f1c8fa49` |

## Scope

TEST20 is a diagnostic sampler baseline, not the finished visual FPS counter.
It:

- discovers `eboot.bin` through the FW 9.60 `KERN_PROC` layout;
- resolves `libSceVideoOut.sprx`;
- reads the VideoOut counter through the FW 9.60 DMAP page walk;
- calculates one-second integer FPS samples;
- follows game PID changes;
- writes one first-sample record for each game PID;
- contains no ShellUI renderer, injection, widget or overlay IPC;
- contains no shutdown recorder or shutdown-time file writes.

No FPS value is expected on screen. The hardware test recorded `60` and `59`
FPS for two games in the log, then completed a normal system-menu restart
without an improper-shutdown warning.

The public, visually complete `v1.0.0` remains the stable user release. The
renderer-enabled v1.1.x experiments are not included in this branch because
their restart regression is unresolved.

## Build

The easiest reproducible build is the GitHub Actions workflow **PS5 Source
Build**. It pins PS5 Payload SDK v0.41 and etaHEN source 2.4B
(`d47f99bd37f349ae59b3c4b66e09e93ba69f56cd`). The tested runtime was etaHEN
2.6; the pinned 2.4B tree is a build dependency.

For local build details, see [BUILDING.md](BUILDING.md). The build fails if the
resulting hashes differ from the tested files.

## Evidence

The runtime contract, hardware result and source-to-artifact proof are in
[docs/PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.md](docs/PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.md).

## License

Common FPS-owned source is licensed under GPL-3.0-or-later. Third-party
projects retain their own licenses and notices.

Homebrew software for modified PlayStation 5 systems. Use at your own risk.
