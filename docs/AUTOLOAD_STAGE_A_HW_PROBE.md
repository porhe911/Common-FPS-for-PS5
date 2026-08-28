# v1.1.0 Safe Autoload — Stage A hardware probe (FW 9.60)

## Purpose

Stage A validates the startup-race fix without yet enabling the final visual
PUI path.

It intentionally separates two health states:

```text
ReceiverReady
    = the source-built ShellUI companion was injected and its loopback
      receiver/heartbeat worker is alive

VisualReady
    = the future ShellUI main-thread Mono/PUI bootstrap is installed and the
      Game/RootWidget scene is safe to use
```

The controller does **not** enter the game FPS lifecycle until `VisualReady`.

Therefore the expected Stage A hardware result is:

- etaHEN autoload returns quickly;
- no early game-process sampling;
- no duplicate rapid renderer injection;
- no intentional SceShellUI kill path;
- the first game should launch without the old autoload startup race;
- **FPS is not expected to appear yet** because Stage B will implement the
  ShellUI main-thread visual bootstrap.

Do not publish Stage A as v1.1.0 stable.

## Files

The source build still produces:

```text
Common_FPS_PS5_v1.1.0.elf
Common_FPS_PS5_etaHEN_v1.1.0.plugin
Common_FPS_ShellUI_v1.1.0.elf
```

The controller currently loads the companion from the first existing path:

```text
/data/CommonFPS/Common_FPS_ShellUI_v1.1.0.elf
/data/etaHEN/plugins/Common_FPS_ShellUI_v1.1.0.elf
```

Recommended test placement:

```text
/data/etaHEN/plugins/Common_FPS_PS5_etaHEN_v1.1.0.plugin
/data/CommonFPS/Common_FPS_ShellUI_v1.1.0.elf
```

Create `/data/CommonFPS/` if it does not exist.

## Test sequence

1. Keep the known-good public v1.0.0 files backed up.
2. Copy the Stage A `.plugin` and companion ELF to the paths above.
3. Enable Common FPS in etaHEN Autoload.
4. Reboot the console.
5. Start etaHEN.
6. Do not manually Stop/Run Common FPS.
7. Launch one known-good PS4 or PS5 test game soon after etaHEN has loaded.
8. Observe only:
   - whether the game starts normally;
   - whether there is a system/application error;
   - whether there is a Kernel Panic;
   - whether etaHEN remains usable after closing the game.
9. Close the first game and launch a second game.
10. Again record stability only.

## Expected Stage A result

```text
autoload enabled
-> etaHEN starts
-> Common FPS parent returns immediately
-> worker waits for stable SceShellUI + Mono
-> source renderer companion is injected
-> ReceiverReady heartbeat confirmed
-> controller waits for VisualReady
-> game lifecycle remains untouched
```

There should be no FPS overlay yet. That is deliberate.

If this probe is stable, Stage B can safely add the main-thread PUI hook and
turn `VisualReady` on only after the actual overlay scene is usable.

## Safety decisions preserved

- no `elfldr_exec()` against SceShellUI;
- no `SIGKILL` fallback;
- one `pt_attach -> elfldr_debug -> pt_detach(pid, 0)` loader session;
- upstream full payload-args preparation remains inside `elfldr_debug()`;
- renderer installation is rate-limited;
- ShellUI PID changes invalidate old heartbeats;
- game sampling is gated behind `VisualReady`.
