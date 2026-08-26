# Common FPS clean ShellUI payload — alpha2

This directory replaces the historical PHU-derived renderer conceptually with
a source-only renderer based on etaHEN's GPL ShellUI/PUI implementation.

Confirmed upstream primitives used by etaHEN:

- `Scene.RootWidget`
- `CreateUIFont(...)`
- `CreateLabel(...)`
- `Widget_Append_Child(...)`
- `FindWidgetByName`
- `RemoveFromParent`

The alpha2 renderer uses a deliberately simple update model:

1. keep the static `FPS:` label;
2. on a changed state packet, remove and recreate only the numeric value label;
3. coordinates are recomputed from the selected corner every update;
4. no game process hooks are used.

The final integration still needs to be wired into etaHEN's ShellUI injection
build and tested on FW 9.60.
