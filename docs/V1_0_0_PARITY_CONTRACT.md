# v1.0.0 parity contract

This file is the **behavioral source of truth** for all future Common FPS
source-built versions.

The public stable baseline is:

```text
Common_FPS_PS5_v1.0.0.elf
SHA-256:
6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c

Common_FPS_PS5_etaHEN_v1.0.0.plugin
SHA-256:
f1240511bfe3cb17a3db6f4d099cf3cd69be678d10901425a7b33911265195e1
```

Internal development branch:

```text
v0.28b Async Stable Init
```

Any source rewrite must preserve the rules below unless a hardware test proves
a replacement is equally or more stable.

---

## 1. FPS output

Public/displayed FPS is **integer only**.

Correct:

```text
FPS: 59
FPS: 60
FPS: 30
```

Forbidden regression:

```text
FPS: 59.6
FPS: 60.0
```

Internal tenths may be used only as an intermediate value.

---

## 2. Overlay appearance

Stable v1.0.0 defaults:

```text
corner      = bottom_left
font_size   = 26
label       = FPS:
label color = #B366FF
value color = #FFFFFF
```

The FPS pair must remain stationary.

The renderer must recalculate/reset its anchor on every state update. Never
allow an accumulating `cursor_x` / horizontal offset.

---

## 3. Game lifecycle

The plugin is persistent.

Required state transition:

```text
WAIT_FOR_GAME
    |
    | find eboot.bin
    v
ACTIVE(pid A)
    |
    | pid A disappears
    v
RESET
    |
    v
WAIT_FOR_GAME
    |
    | find eboot.bin
    v
ACTIVE(pid B)
```

Required user-visible behavior:

```text
Game A -> close -> Home -> Game B
```

without restarting the plugin and without KP.

Never keep using module addresses from a dead PID.

---

## 4. FPS backend

FPS access is read-only.

Do not add game rendering hooks.

Do not patch game memory.

Stable source model:

```text
eboot.bin
    |
libSceVideoOut.sprx
    |
base + 0x34980
    |
7 probe records x 0x18
    |
first enabled record with pointer
    |
read root pointer
    |
root + 0x768
    |
uint32 counter
```

FPS is calculated from counter delta and elapsed monotonic time.

---

## 5. ShellUI renderer

The overlay belongs in ShellUI/PUI, not in the game process.

Do not reintroduce:

- PHU game hooks;
- fan/temperature logic;
- game-memory writes;
- kstuff FPS hooks;
- runtime config polling in the render path;
- debug notification spam.

Future user configuration may change corner/font size, but the default must
remain the v1.0.0 layout.

---

## 6. Startup behavior

The final stable v1.0.0 behavior came from `v0.28b`.

The important rule is:

```text
main
  |
  +-- fork()
       |
       +-- parent -> return immediately to etaHEN/ELF loader
       |
       +-- child  -> run the FULL stable initialization
```

The parent returning quickly is what makes activation feel immediate.

The child must still perform the complete safe initialization.

---

## 7. Mandatory initialization behavior

The following experimental optimizations were rejected and must NOT silently
return in the clean rewrite.

### Do not remove `elfldr_payload_args()`

Experiment:

```text
v0.28a Zero Args Fast
```

Result:

- activation became almost instant;
- starting a game froze the console;
- Kernel Panic occurred.

Conclusion:

`payload_args` has required side effects even if the visible renderer argument
looks unused.

A clean source-built injector may replace this mechanism only after hardware
testing proves the replacement safe.

### Do not remove Scene readiness

Experiment:

```text
v0.27a
```

Result:

- FPS overlay disappeared.

Conclusion:

the ShellUI renderer must not install before its required Scene/PUI state is
ready.

### Do not break one loader session using middle `PT_DETACH`

Experiments:

```text
v0.27b
v0.27c
```

Result:

- activation became shorter;
- renderer did not appear.

Conclusion:

do not assume a remote ELF loader can detach in the middle and recreate all
required state later.

---

## 8. Accepted final behavior

The v1.0.0 baseline was accepted only after all of these were true:

- fast plugin activation;
- real FPS;
- integer-only display;
- fixed bottom-left position;
- font size 26;
- no horizontal drift;
- no debug spam;
- one game can close;
- another game can launch;
- counter continues without another Run;
- no KP in the tested lifecycle.

These are release-blocking requirements for a source-built successor.


---

## 9. Activation scope discovered after v1.0.0 release

The accepted v1.0.0 stability baseline applies to **manual activation after
etaHEN/ShellUI is fully loaded**.

Hardware testing later showed that etaHEN autoload can start v1.0.0 too early.
That path can leave the plugin apparently enabled without a live FPS renderer
and can lead to an error/KP when a game is launched.

Therefore:

- do not weaken any v1.0.0 manual-start behavior;
- do not describe v1.0.0 autoload as stable;
- v1.1.0 must add a readiness/retry guard rather than changing the stable FPS
  lifecycle itself.
