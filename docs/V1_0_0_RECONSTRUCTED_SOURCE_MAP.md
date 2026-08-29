# v1.0.0 reconstructed runtime source map

This document records facts recovered from the hardware-stable public
`v1.0.0` binary and the exact PHU Games Tools v1.14.25-r11 donor supplied by
the project owner.

The goal is source parity first. Autoload changes come only after a source-built
manual-start candidate reproduces v1.0.0 on hardware.

## 1. Controller entry point

The stable `main` is fully recovered from the binary:

```cpp
int main() {
    const pid_t pid = fork();
    if (pid > 0)
        return 0;
    return probe_main_entry();
}
```

This is the v0.28b asynchronous activation change. The parent returns to the
loader immediately and the child keeps the stable initialization/lifecycle.

## 2. Stable controller -> renderer wire contract

The stable controller and stable renderer use UDP loopback, not the later
clean-room `CFPS` packet used by the current rewrite.

- address: `127.0.0.1`
- port: `55541`
- datagram size: `0x420` bytes
- magic: `PHUF` (`0x46554850` on the little-endian target)
- version: `1`

Exact DWARF-recovered layout:

```cpp
struct PhuSharedFps {
    uint32_t magic;       // +0x00, PHUF
    uint32_t version;     // +0x04, 1
    uint64_t seq;         // +0x08
    double   fps;         // +0x10
    uint64_t last_ns;     // +0x18
    char     text[1024];  // +0x20
};                       // sizeof = 0x420
```

The public reconstruction uses equivalent names in
`include/common_fps/v1_stable_wire.hpp` (`sequence` for the recovered `seq`).
Numeric packets have `text[0] == 0`. Loading packets use the exact controller
literal `"FPS\tloading\n"` with `fps == 0.0`.

## 3. Stable FPS sampler

`probe_main_entry` calls the unchanged donor `inject_main_entry()` first, then
creates one UDP socket and runs the game lifecycle.

Target discovery:

- process name: `eboot.bin`
- module: `libSceVideoOut.sprx`
- table offset: `0x34980`
- table entries: `7`
- entry size: `0x18`
- first usable entry requires non-zero flag at `+0x00` and pointer at `+0x08`
- dereference the pointer
- frame/vsync counter is `dereferenced_base + 0x768`
- counter width: `uint32_t`

Sampling arithmetic recovered instruction-for-instruction:

```text
delta_counter = uint32_t(current - previous)
elapsed_us    = seconds_delta * 1,000,000 + microseconds_delta
tenths        = delta_counter * 10,000,000 / elapsed_us
reject if tenths > 3000
fps           = tenths / 10.0
```

The controller samples at approximately one-second intervals. If target
resolution or sampling fails, it sends a loading packet, resets the target, and
retries.

The stable renderer formats numeric values with `FPS\t%.0f\n`, so the visible
FPS is integer-only even though the wire value retains tenth-FPS precision.

### 3.1 Stable game-memory reads are DMAP reads, not per-read ptrace

The historical v1.0.0 outer payload retains `proc_rw_v940.cpp`. Every VideoOut
table/root/counter read in `probe_main_entry` goes through:

```cpp
prw::proc_read(pid, virtual_address, output, size)
```

Recovered `prw::proc_read()` behavior:

1. zero-length read succeeds immediately;
2. translate the current game virtual address to a physical address and return
   the mapping page size;
3. calculate the number of bytes remaining before the current mapping boundary;
4. copy `min(remaining, bytes_to_boundary)` from
   `g_dmap_base + physical_address` with `kernel_copyout()`;
5. advance source/destination and repeat until the whole request is copied;
6. fail immediately on address-translation or physical-copy failure.

The translator supports the page sizes observed in the donor implementation:
`4 KiB`, `2 MiB` and `1 GiB`.

This distinction is a parity requirement. The later clean rewrite's
`pt_attach() -> pt_copyout() -> pt_detach()` for every read is **not** the
stable v1.0.0 memory path and must not be used by the reconstructed parity
runtime.

The host-testable page-chunk algorithm is published as
`v1_stable::proc_read_dmap()` in `include/common_fps/v1_stable_memory.hpp` and
`src/reconstruction/v1_stable_memory.cpp`. The PS5-specific translator/DMAP
backend is reconstructed separately from the donor `proc_rw_v940.cpp` behavior.

## 4. Stable shsrv loader delta

The outer PHU r11 payload and Common FPS v1.0.0 have identical layout and Build
ID. Only three outer functions contain runtime text changes: `main`,
`probe_main_entry`, and `elfldr_load`.

The two `elfldr_load` edits are decoded against pinned shsrv
`6f320637d56d344a0e7797753099e33238bbf146`.

### 4.1 Continue during local ELF preparation

Immediately after the remote `pt_mmap()` reservation succeeds, stable v1.0.0
preserves the returned mapping address and resumes the traced ShellUI target
with the equivalent of `PT_CONTINUE`.

The return value from `pt_mmap()` is preserved exactly; failure handling is
unchanged.

Purpose: the target is not left frozen while the loader performs local ELF
mapping/copy/relocation preparation.

### 4.2 Re-stop before the next trace-sensitive remote phase

Before the later remote protection/commit phase, stable v1.0.0 synchronously
stops the same target and waits for the stop to complete, then continues through
the original shsrv logic.

This is the recovered "Trace Continue" contract: one loader session, temporary
continue during heavy local preparation, then re-stop before trace-sensitive
remote operations. It is **not** an intermediate detach/re-attach cycle.

The original `elfldr_payload_args()` phase is retained after the image load.
Removing payload args was hardware-rejected in the historical v0.28a test.

## 5. Stable renderer delta from PHU r11

The embedded renderer is only 145 runtime bytes different from the exact PHU
r11 donor (`120` bytes `.text`, `25` bytes `.rodata`). Changed text maps only
to:

- `elf_main`
- `read_cfg_v13`
- `phu_set_fps_text_raw`
- `phu_create_fps_label`
- `install_onrender_hook`
- `OnRender_Hook`

Recovered behavior:

### `elf_main`

- skips PHU-specific pre-Mono startup/diagnostic work that Common FPS does not
  need;
- removes the donor's fixed `sleep(3)` completely;
- retains root-domain/thread attach;
- retains `mono_glue_init()`;
- retains the up-to-60-attempt Scene readiness loop with one-second retry;
- retains the donor's original `install_onrender_hook()` path;
- binds UDP loopback port `55541`;
- skips donor runtime-config startup work and enters the PHUF receive loop.

### `read_cfg_v13`

Stable defaults disable unrelated PHU overlays/features and use FPS font size
`26`. Stable bypasses renderer config-file parsing, making these defaults fixed
for the release.

### visible FPS

- label text: `FPS:`
- loading text: `FPS: loading`
- numeric format: integer (`%.0f`)
- label color: `#B366FF`
- value color remains white
- font size: `26`
- bottom-left placement uses recovered fixed cursor/anchor values rather than
  accumulating the cursor each update.

### `install_onrender_hook`

Stable bypasses PHU's optional `patch_main_thread_check()` stage but preserves
PHU r11's original hook installation path. This distinction is important: the
hardware-rejected Stage B/B2/B3 experiments attempted to build a second
independent/chained update hook and are not parity targets.

### `OnRender_Hook`

Stable removes the PHU controller-combo polling and runtime config live-reload
calls from the render callback. The remaining donor render/update path is kept.

## 6. Reconstruction order

1. exact PHUF wire contract + sampler arithmetic (host-tested);
2. stable DMAP `proc_read` contract and PS5 translator backend;
3. stable controller lifecycle and process/module probe;
4. stable shsrv loader delta on top of pinned upstream source;
5. source reconstruction of the six changed renderer functions on top of the
   donor architecture and public upstream helpers;
6. source-built manual-start hardware parity on FW 9.60;
7. only then add an Autoload readiness gate around the proven initialization.

No new hardware artifact should be promoted until step 6 passes.
