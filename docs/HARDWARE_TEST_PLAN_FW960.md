# FW 9.60 hardware acceptance plan

Stable reference: public Common FPS v1.0.0.

## A. Manual activation baseline

1. Boot/jailbreak.
2. Activate etaHEN.
3. Wait until Home/ShellUI is fully usable.
4. Manually Run Common FPS.

Expected:
- fast Run;
- no debug spam;
- integer FPS;
- no KP.

## B. Autoload — new v1.1.0 release blocker

Enable Common FPS autoload and cold boot through the normal etaHEN flow.

Expected:
- no error on the first game launch;
- no KP;
- FPS appears without manual Stop/Run;
- etaHEN "enabled" state corresponds to a live renderer.

Repeat at least five cold-start cycles.

## C. Early game launch race

With autoload enabled, launch a game as soon as the Home UI becomes available.

Expected:
- guard may delay FPS appearance;
- game must still launch normally;
- plugin must not touch the game until renderer/runtime readiness is confirmed;
- FPS appears automatically once safe.

## D. 30/60 behavior

Expected:
- quality mode ~ `FPS: 30`;
- performance mode ~ `FPS: 60`;
- no decimal places.

## E. Position regression

Run at least five minutes.

Expected:
- bottom-left default;
- font size 26;
- no horizontal drift;
- no disappearing counter.

## F. Game lifecycle

Repeat at least five cycles:

```text
Game A -> close -> Home -> Game B
```

Expected:
- no second Run;
- new PID discovered;
- no stale module address;
- no KP.

## G. Renderer self-heal

If practical, stop/restart the renderer-side component or recreate ShellUI.

Expected:
- worker does not remain falsely "enabled but dead";
- guard retries and restores FPS;
- no duplicate labels.

## H. Standalone ELF

The standalone ELF must remain independent of etaHEN's built-in FPS setting.

## Promotion rule

Do not replace public v1.0.0 until all manual, autoload, lifecycle and
standalone tests pass on FW 9.60.
