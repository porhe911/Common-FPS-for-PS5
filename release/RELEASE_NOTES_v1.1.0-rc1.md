# Common FPS for PS5 — v1.1.0-rc1

> First public source-built release-candidate branch.

## Highlights

- GPL-3.0-or-later source publication
- integer-only FPS
- fixed bottom-left compatibility default
- font size 26
- read-only VideoOut sampler model
- persistent game lifecycle
- Safe Autoload state machine
- four-corner configuration model
- no PHU binary blob in the source-built path
- automated host tests
- GitHub source-build workflow

## FW 9.60

The stable manual-start behavior of v1.0.0 is retained as the compatibility
baseline.

Safe etaHEN autoload is new in v1.1.x and requires hardware validation before
promotion to stable.

## Status

Release Candidate. Do not replace v1.0.0 as the recommended stable binary until
the FW 9.60 hardware checklist passes.
