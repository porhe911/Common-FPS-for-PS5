# Common FPS clean ShellUI payload — Safe Autoload Stage A

This directory is the source-only ShellUI side of Common FPS. It is built
against etaHEN's GPL ShellUI support source; no historical PHU renderer binary
blob is used.

## Stage A

The injected entry point currently does only two safe background tasks:

1. bind the Common FPS loopback state receiver;
2. emit a `ReceiverReady` heartbeat to the controller.

It deliberately does **not** call PUI widget methods from the injected worker
thread.

Upstream ShellUI research shows that UI construction/update methods can require
the ShellUI main thread. Calling them from an arbitrary injected thread can
raise a managed fatal exception.

The controller therefore distinguishes:

- `ReceiverReady`: companion is alive;
- `VisualReady`: main-thread Mono/PUI bootstrap is installed and the visual
  scene is safe.

The normal game FPS lifecycle is gated on `VisualReady`.

## Stage B remaining work

The next stage must:

1. resolve/attach to the existing ShellUI Mono runtime;
2. obtain the required PUI image(s);
3. install a minimal detour on a ShellUI main/update-thread method;
4. acquire the `Game`/`RootWidget` scene from that main thread;
5. run `apply_latest_state()` only from that safe hook;
6. emit `VisualReady` only after the above succeeds;
7. self-heal if the scene is replaced.

Confirmed renderer primitives already used by the source renderer include:

- `Scene.RootWidget`
- `CreateUIFont(...)`
- `CreateLabel(...)`
- `Widget_Append_Child(...)`
- `FindWidgetByName`
- `RemoveFromParent(...)`

No game-process hooks are used for drawing.
