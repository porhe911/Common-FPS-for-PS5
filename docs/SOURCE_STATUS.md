# Source status

This branch is the byte-reproducible source snapshot for PARITY TEST20.

Verified Release builds reproduce both submitted artifacts exactly:

```text
b791baf6f85063d0c7ec57eebcbf60116dff58dfc4ec74aa6d9dedcc1ecc32ef  ELF
7bdcc16cc582bd89c7f1680471436ef58fdc53bab159c7e36278a5e8f1c8fa49  plugin
```

The snapshot includes the controller, FW 9.60 process discovery, VideoOut
module lookup, DMAP reader, FPS calculation and exact etaHEN wrapper metadata.
It does not include a linked or embedded ShellUI renderer.

TEST20 passed one two-game lifecycle and normal-restart hardware run on FW
9.60 with etaHEN 2.6. It is a diagnostic sampler baseline, not a complete
visual FPS release.

See [PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.md](PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.md).
