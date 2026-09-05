# v1.1.1 sleep-recovery candidate

This candidate keeps the published v1.1.0 sampler and renderer path, and adds
one lifecycle correction: the controller compares the current `SceShellUI` PID
with the previous observation. If ShellUI disappears or is recreated, it marks
the renderer offline and allows the normal per-PID bootstrap to run again.

The stable v1.1.0 release is unchanged. This branch is a separate hardware
test candidate and must not replace v1.1.0 until Rest Mode validation passes.

## Candidate files

```text
Common_FPS_PS5_v1.1.1_SLEEP_RECOVERY.elf
Common_FPS_PS5_etaHEN_v1.1.1_SLEEP_RECOVERY.plugin
Common_FPS_ShellUI_v1.1.1_sleep_recovery.elf
SHA256SUMS_v1.1.1_SLEEP_RECOVERY.txt
```

The renderer ELF is embedded in the controller. It is included separately for
audit and reproducible-build comparison; do not load it in addition to the
plugin.

## Controlled Rest Mode test

1. Boot the PS5 cleanly and enable only this candidate plugin in etaHEN.
2. Start one PS4 or PS5 game and wait for a real integer FPS value.
3. Enter Rest Mode from the normal system menu for at least one minute.
4. Wake the console and return to the same game. Allow up to 15 seconds for
   ShellUI and the renderer to become ready; the value may show `Loading` first.
5. Confirm that the FPS value returns without reactivating the plugin.
6. Repeat the sleep/wake cycle three times, then perform a normal system-menu
   restart.

The controller log is:

```text
/data/CommonFPS_v111_sleep_recovery.log
```

After a ShellUI restart, the log should contain a line like:

```text
ShellUI lifecycle previous_pid=... current_pid=... transition=replaced renderer_reset=1
```

followed by a new `ShellUI inject start`, `ShellUI renderer online`, and a
valid `Sampler online` record. Record the firmware, etaHEN version, whether the
overlay returned after each wake, and whether the final restart was clean.

The existing source constants were proven on PS5 FW 9.60 with etaHEN 2.6.
Other firmware versions remain compatibility experiments until separately
validated.
