# Common FPS for PS5 — v1.1.0

> First public hardware-validated source-built release.

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

Safe etaHEN autoload is new in v1.1.x. The tracked-process renderer and visible
FPS counter have now passed the hardware validation gate.

## Status

Stable v1.1.0 source snapshot. The visually complete v1.0.0 release remains
available as a compatibility fallback.
