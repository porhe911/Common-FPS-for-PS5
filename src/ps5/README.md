# PS5 adapter status

This directory is the only remaining hardware-specific part of the clean rewrite.

The Common FPS core is source-built and host-tested. The PS5 adapter must provide:

1. `Platform::find_game_process()`
2. `Platform::process_alive()`
3. `Platform::find_module()`
4. read-only `Platform::read_memory()`
5. ShellUI/PUI `Renderer`

## Upstream source basis

Use GPL source from:

- PS5 Payload SDK
- etaHEN Plugin SDK
- etaHEN ShellUI source

Do **not** copy the historical PHU-derived binary renderer back into this tree.

etaHEN's published ShellUI source already demonstrates:

- Mono method hooking;
- adding label widgets under `Scene.RootWidget`;
- explicit Top Left / Top Right / Bottom Left / Bottom Right positions;
- creating/removing overlay labels.

Common FPS only needs two persistent labels:

```text
FPS:
59
```

or one formatted logical pair, depending on renderer implementation.

The next hardware milestone is to implement this adapter and compile it with
the PS5/etaHEN SDKs.
