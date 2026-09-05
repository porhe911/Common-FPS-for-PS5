# PARITY TEST30: etaHEN-tracked process, no internal fork

## Purpose

TEST20 completed one clean hardware restart. TEST21 and TEST29 later produced
an improper-shutdown warning. Those experiments also showed that the renderer
alone is not a sufficient explanation, because TEST29 had no visible overlay.

The etaHEN plugin loader already spawns a dedicated process and records that
PID in `/system_tmp/CFPS00049.PID`. TEST20 through TEST29 then called `fork()`
again: the recorded parent returned, while the sampler continued in a child
that etaHEN no longer tracked. TEST30 tests whether that stale process model
creates a shutdown race.

This is a candidate explanation, not a confirmed root cause. The successful
TEST20 restart was only one run, and the stable visual v1.0.0 also used a
historical fork-first startup path.

## Exact A/B boundary

TEST30 is built from the byte-reproduced TEST20 source. The only behavioral
change is:

```text
TEST20: etaHEN-spawned process -> fork -> parent exits -> child samples
TEST30: etaHEN-spawned process -----------------------> process samples
```

Unchanged:

- FW 9.60 `KERN_PROC` discovery;
- VideoOut module lookup and Auth-ID restoration;
- read-only DMAP counter sampling once per second;
- reattachment after a game PID change;
- one first-FPS log record per game PID;
- no renderer, injector, overlay IPC or shutdown recorder;
- dynamic dependencies: `libkernel_sys`, `libSceLibcInternal`, `libSceNet`.

## Artifacts

```text
732db78e9329fcabbe93a66972e5d20e5137256769385138348cc88c45a99f9a  Common_FPS_PS5_v1.1.0_PARITY_TEST30_TRACKED_PROCESS_NO_FORK_AB.elf
b766d1c479cf5720b723fa73caab442bece4048420ed371a70ba6378b49b9515  Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST30_TRACKED_PROCESS_NO_FORK_AB.plugin
```

Plugin metadata: `CFPS00049 / 1.38`. The 29-byte etaHEN header is followed by
the ELF byte-for-byte.

## Hardware procedure

1. Start from a fresh boot and make sure TEST20, TEST21 and TEST29 are disabled.
2. Install and activate only the TEST30 plugin.
3. Start one game and wait at least 15 seconds.
4. Close it, start a second game and wait at least 15 seconds.
5. Use **Restart PS5** from the normal system menu while TEST30 is still active.
6. After boot, report whether the improper-shutdown warning appeared and send
   `/data/CommonFPS_v110_test30_tracked_process.log` in full.

No on-screen FPS counter is expected in this diagnostic build.

Expected beginning of the log:

```text
Common FPS v1.1.0 PARITY TEST30 tracked-process no-fork A/B
Mode=loader-tracked internal_fork=absent spawned_pid=resident ...
Worker ready pid=... ppid=... first_shellui_pid=...
Sampler online pid=... counter=... first_fps=...
```

## Interpretation

- Clean restart: keep the tracked-process lifecycle and next add the renderer
  on top of it in a separate integration test.
- Improper restart: the one clean TEST20 run was not a sufficient stability
  proof; retain the open TEST20 sampler baseline and investigate sampler
  quiescing/detachment rather than renderer IPC alone.
