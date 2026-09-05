# PS5 v1.1.0 target

The active PS5 CMake target builds the TEST30 tracked worker and restores the
proven source-built ShellUI renderer. Common FPS does not call `fork()` after
etaHEN has already spawned the plugin process.

- FW 9.60 `KERN_PROC` game discovery;
- VideoOut module discovery with mandatory Auth-ID restoration;
- read-only DMAP sampling;
- one-second FPS calculation and game-PID reattachment;
- startup and first-sample-per-game logging.
- source-built shared ShellUI renderer with pinned Mono/PUI handles;
- target-stack bootstrap and one-way loopback UDP overlay IPC.

The shutdown recorder and all explicit stop/unload paths remain disabled.
The expected on-screen value is an integer `FPS: 59`/`FPS: 60` after the
sampler warm-up.

`tools/verify_test31_artifact.py` confirms wrapper metadata, exact hashes,
embedded renderer markers, the no-fork controller boundary and log path.
