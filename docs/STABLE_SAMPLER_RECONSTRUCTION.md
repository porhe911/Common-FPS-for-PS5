# Stable v1.0.0 FPS sampler reconstruction

The clean rewrite initially assumed `libSceVideoOut.sprx + 0x768` directly
contained an FPS floating-point value.

Disassembly of the released stable v1.0.0 ELF showed that assumption was
incorrect.

## Actual v1.0.0 algorithm

The stable binary performs this chain:

```text
find eboot.bin
    |
resolve libSceVideoOut.sprx
    |
module base + 0x34980
    |
read 0xA8 bytes
    |
7 records x 0x18 bytes
    |
first record:
    uint32 @ +0x00 != 0
    uint64 @ +0x08 != 0
    |
read uint64 from that pointer
    |
+ 0x768
    |
read uint32 counter
```

It takes an initial counter/time sample, waits, reads again, then calculates:

```text
delta = current_counter - previous_counter

tenths_fps =
    delta * 10,000,000 / elapsed_microseconds

fps =
    tenths_fps / 10.0
```

The stable binary rejects intermediate values greater than `3000`
(300.0 FPS).

## Why this matters

`+0x768` is a counter, not an FPS double.

Alpha2 updates the published source model to match the released binary's actual
read-only algorithm.

No game-memory writes are involved.


## Public Common FPS output

The intermediate `tenths_fps` value is an implementation detail only.

The final Common FPS requirement is integer-only output:

```text
integer_fps = round(tenths_fps / 10)
```

No decimal FPS is sent to the renderer.
