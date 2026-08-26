# etaHEN autoload regression — FW 9.60

## Hardware observation

Stable public Common FPS v1.0.0 behaves correctly when manually started after
etaHEN has fully loaded.

A separate etaHEN autoload test produced this sequence:

```text
etaHEN autoload enabled
    ->
Common FPS starts during etaHEN startup
    ->
game launch
    ->
application/error condition
    ->
Kernel Panic
```

After reboot and etaHEN activation, the plugin could still appear enabled while
no FPS overlay was present.

Manually performing:

```text
Stop
-> Run
```

after etaHEN was fully loaded restored the FPS overlay.

## Interpretation

This does not invalidate the accepted manual-start stability of v1.0.0.

It identifies a new startup race:

- etaHEN's persistent/plugin state can say "enabled";
- the Common FPS renderer worker may have failed during early startup;
- the old worker had no self-healing autoload state machine.

## v1.1.0 requirement

Safe autoload is now a release-blocking requirement.

The source-built successor must:

1. return quickly to etaHEN;
2. keep its worker alive;
3. wait for runtime readiness;
4. require consecutive stable readiness checks;
5. install the renderer only after that gate;
6. retry after failed installation;
7. confirm renderer health after installation;
8. reinstall if the renderer later disappears;
9. avoid touching the game until the renderer side is healthy.

This is implemented in Alpha 5 by `AutoloadGuard`.

## Current v1.0.0 recommendation

Until a source-built successor passes hardware autoload tests:

```text
Start Common FPS manually after etaHEN is fully loaded.
```
