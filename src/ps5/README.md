# PS5 target — v1.2.0

The PS5 target builds the tracked Common FPS controller, etaHEN plugin wrapper
and embedded ShellUI component.

The current implementation includes:

- `KERN_PROC` game discovery;
- game PID lifecycle tracking;
- VideoOut module discovery;
- read-only VideoOut counter validation;
- adaptive `/dev/dce` FPS fallback;
- 60-second VideoOut backoff while DCE is healthy;
- immediate recovery after repeated DCE misses;
- ShellUI PID tracking and reinjection;
- source-built PUI overlay;
- one-way loopback UDP state transport;
- integer-only FPS display;
- 24px default overlay font.

The FPS sampler does not write to game memory.

FW 4.51 is hardware-confirmed with PS4 and PS5 games, 30/60 FPS, Rest Mode
recovery and normal reboot. FW 9.60 remains a hardware-tested baseline.
Other firmware families must be treated according to the support table in the
top-level README.

The shutdown recorder and explicit shutdown-time write paths remain disabled.
