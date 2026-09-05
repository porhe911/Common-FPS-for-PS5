# v1.1.0: hardware-validated tracked-process renderer

The tracked-process base established a clean process boundary: the
etaHEN-created worker stayed resident, sampled two games, and the PS5 restarted
normally. The renderer was then added on that base and passed the combined
hardware gate.

## What v1.1.0 contains

- the tracked worker (`internal_fork=absent`);
- the same FW 9.60 VideoOut counter sampler and read-only DMAP walk;
- the source-built shared ShellUI ELF embedded once in the controller;
- the hardware-tested target-stack bootstrap (`PT_ATTACH`, loader, payload
  arguments, remote `pthread_create`, `PT_DETACH`, original-auth restore);
- the pinned-Mono/PUI renderer and one-way loopback UDP state packets;
- no shutdown recorder, custom signal handler, Stop/helper ELF, unhook,
  unload, injected-thread return or periodic diagnostic writes.

The controller itself never calls `fork()`. The renderer's one resident thread
is created inside ShellUI by the proven bootstrap; this is why the artifact is
named “no internal fork”, not “no thread”.

## Artifacts

```text
dist/Common_FPS_PS5_v1.1.0.elf
dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin
dist/Common_FPS_ShellUI_v1.1.0.elf
```

Plugin metadata:

```text
Title ID: CFPS00050
Version:  1.39
```

The build script runs `tools/verify_v1_1_0_artifact.py`, checks that `main`
contains no `fork` call, and writes `dist/SHA256SUMS.txt`.

## Controlled PS5 run

1. Boot the PS5 cleanly and wait for any storage check to finish.
2. Disable every other Common FPS/PHU plugin and autoload entry. Do not load
   v1.1.0 twice in one boot; do not load an earlier build in the same boot.
3. Activate exactly one v1.1.0 form. Prefer the `.plugin`; use the `.elf` only
   as the alternative loader form, never both.
4. Wait for the short ShellUI bootstrap. The first overlay may say
   `FPS: Loading`; this is expected until the sampler has a baseline and one
   discarded warm-up delta.
5. Start Game A. After about one minute the bottom-left overlay should show an
   integer such as `FPS: 59` or `FPS: 60`, and the controller log should gain
   one `Sampler online` line.
6. Close Game A fully, start Game B without reactivating v1.1.0, and confirm a
   second live integer and a second `Sampler online` line.
7. Close Game B and use the normal system-menu **Restart PS5**. Do not use a
   physical-button power cut, Stop/helper ELF, or remove
   `/data/phu_overlay.lock`.
8. On the next boot, copy the complete controller log before activating any
   new test:

```text
/data/CommonFPS_v110.log
```

Also report whether the overlay was visible in both games and whether the
restart completed automatically without an improper-shutdown warning.

## Confirmed hardware result

The submitted run completed the renderer bootstrap and displayed real FPS in
two games: `first_fps=59` and `first_fps=60`. The user then performed repeated
normal system-menu restarts; the console did not show an improper-shutdown
warning. The complete retained log is
`docs/evidence/V1_1_0_HARDWARE_20260905.log`.

## Expected controller log shape

```text
Common FPS v1.1.0 tracked-process renderer
Mode=loader-tracked internal_fork=absent spawned_pid=resident ... renderer=shared_elf_etaHEN_update_hook injection=target_stack_pthread ipc=udp_loopback_1s mono_gc=pinned ...
Worker ready pid=... first_shellui_pid=... size_rc=0 data_rc=0 ... malformed=0 ...
ShellUI lookup pid=... via=sysctl_tdname ...
ShellUI inject start pid=... payload_size=...
ShellUI bootstrap auth=1/1 attach=1 load=1 args=1 target_stack=1 thread=1 rc=0 detach=1 trace=1/1 imports=.../0 first=none ...
ShellUI renderer online pid=...
Sampler online pid=... first_fps=...
Sampler online pid=... first_fps=...
```

This is the promoted v1.1.0 source snapshot. Keep the public v1.0.0 release
available as a fallback for rollback if a future runtime environment differs.
