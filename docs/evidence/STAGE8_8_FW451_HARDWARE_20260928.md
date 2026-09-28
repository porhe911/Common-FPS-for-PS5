# Stage 8.8 hardware evidence — FW 4.51 — 2026-09-28

This record preserves the hardware result that motivated the Stage 8.9
standby optimisation.

## Result

The tester confirmed visible integer FPS in both a PS5 game and a PS4 game on
PS5 system software 4.51.

Observed on-screen values included:

- 30 FPS in GTA V;
- 60 FPS in another tested title.

The controller remained resident, the ShellUI renderer came online, and the
fallback FPS source recovered across game-process changes.

## Submitted controller logs

Session A:

`CommonFPS_universal_stage8_8 (1).log`

SHA-256:

`6fa9f142e8724eb3e8a1732e1334247906f788b4d04e3c5b87cd93acb8160c2a`

Session B:

`CommonFPS_universal_stage8_8.log`

SHA-256:

`07aee44fc6b7ec34987936fca4d1e23c437ac450ca228a452c1172b0e3d1bb89`

## Key evidence

The FW family was detected as `sdk=0x04510001` and the process-memory reader
used the MDBG backend.

The direct/indirect VideoOut discovery did not validate a counter on the tested
4.51 game processes, which caused the controller to fall back to DCE.

The DCE device opened normally:

```
DCE device online fd=7 open_mode=normal-rw
DCE query online command=0x80308217 auth_retry=0 ...
```

The adaptive DCE logic selected a working counter and produced real FPS:

```
DCE adaptive counter selected offset=0x10 streak=2 fps=60
Fallback sampler online backend=dce-adaptive offset=0x10 first_fps=60
```

A later game process produced 30 FPS through the same adaptive fallback:

```
DCE adaptive counter selected offset=0x10 streak=2 fps=30
Fallback sampler online backend=dce-adaptive offset=0x10 first_fps=30
```

The ShellUI injection and stopped one-byte guard completed successfully:

```
ShellUI bootstrap ... rc=0 detach=1 ...
ShellUI hook request ... phase=probe ... status=0 ... verified=1 ...
ShellUI hook request ... mode=thread-guard ... status=0 ... verified=1 ...
ShellUI renderer online ...
```

The longer session also showed recovery after a DCE ioctl failure and later
selected another adaptive field before returning valid FPS. This is why
Stage 8.9 keeps DCE as the active fallback but periodically re-probes the
preferred VideoOut path instead of continuously performing expensive scans.

## Stage 8.9 consequence

Once DCE is producing valid FPS for the current game PID, Stage 8.9 places
VideoOut discovery into a 60-second standby. It wakes immediately after three
consecutive DCE misses and resets all standby state when the game PID changes.
This preserves recovery while removing the repeated multi-thousand-candidate
VideoOut scans seen in the Stage 8.8 hardware log.
