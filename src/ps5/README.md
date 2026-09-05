# PS5 TEST20 target

The active PS5 CMake target builds only the PARITY TEST20 controller:

- FW 9.60 `KERN_PROC` game discovery;
- VideoOut module discovery with mandatory Auth-ID restoration;
- read-only DMAP sampling;
- one-second FPS calculation and game-PID reattachment;
- startup and first-sample-per-game logging.

ShellUI renderer, injection, overlay IPC and shutdown recorder sources are not
linked into this target. No on-screen FPS counter is expected.

The build is accepted only when
`tools/verify_test20_repro.py` confirms the two hardware-tested hashes.
