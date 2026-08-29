# PHU v1.14.25-r11 -> Common FPS v1.0.0 donor delta

This document records the binary relationship between the public PHU Games Tools
v1.14.25-r11 payload and the hardware-stable Common FPS v1.0.0 payload.

The purpose is source reconstruction. The PHU release archive is **not** treated
as corresponding source and is not committed to this repository.

## Inputs

PHU Games Tools v1.14.25-r11:

```text
phu_overlay.elf
size:   1,555,144 bytes
SHA256: 8e20deefb9100705be8352dc6acb47241c6a044b93dc3f578f93c424789b2622
Build ID: 4fcee90fdb9656b3ff5026338c0846de
```

Common FPS v1.0.0 payload ELF:

```text
size:   1,555,144 bytes
SHA256: 6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c
Build ID: 4fcee90fdb9656b3ff5026338c0846de
```

The two outer ELFs therefore retain the same image layout and the same Build ID.
The PHU ELF is not stripped and preserves the function / compilation-unit map
needed to identify the Common FPS modifications.

## Outer payload delta

Comparing the allocatable outer `.text` section shows:

```text
changed bytes: 1,350
changed runs:  45
```

Only three original functions contain changed text bytes:

```text
main
probe_main_entry
elfldr_load
```

This is a major reconstruction constraint: the stable Common FPS controller did
not require a new payload architecture. It is a very small delta over the exact
PHU r11 controller layout.

Recovered interpretation from the historical v0.27/v0.28 hardware tests:

- `main`: asynchronous `fork()` entry; parent returns immediately and child runs
  the proven initialization;
- `probe_main_entry`: Common-FPS-only lifecycle / VideoOut sampling path;
- `elfldr_load`: Trace Continue behavior required to keep one ShellUI loader
  session alive while local ELF preparation occurs.

The corresponding-source reconstruction should reproduce these behaviors in
normal source instead of preserving the historical binary patches.

## Embedded ShellUI renderer

Both payloads contain an embedded renderer at the same symbol range:

```text
_binary_libphu_overlay_so_start
_binary_libphu_overlay_so_end
size: 260,248 bytes
```

PHU r11 renderer:

```text
SHA256:   8d1490ac7d1ba589044108675e29a1d3cc05a0a946556d52aeb502d114ea7eca
Build ID: c6ee2731d817d85466e9fbef5be7de79
```

Common FPS v1.0.0 renderer:

```text
SHA256:   3640cbcaf907bc900e257503b3c8922fdbc129c492a76a2348fd75c84f68b91c
Build ID: c6ee2731d817d85466e9fbef5be7de79
```

The PHU renderer is unstripped and contains symbol + DWARF information. The
Common FPS copy is stripped, but both retain the same runtime section layout.

Most importantly, comparing only allocatable renderer sections gives:

```text
.text   : 120 changed bytes
.rodata :  25 changed bytes
```

The changed renderer text maps only to:

```text
elf_main
read_cfg_v13
phu_set_fps_text_raw
phu_create_fps_label
install_onrender_hook
OnRender_Hook
```

This proves that the hardware-stable Common FPS renderer is a minimal derivative
of the PHU r11 renderer rather than the independent ShellUI renderer attempted
in the rejected Stage B/B2/B3 branches.

## Renderer source map recovered from PHU DWARF

The unstripped donor renderer preserves these compilation units:

```text
source/main.cpp
source/mono_glue.cpp
source/hook.cpp
source/Detour.cpp
source/hde64.cpp
source/phu_overlay_kernel_rw.c
source/phu_shellui_stubs.c
source/phu_fw_offsets.c
source/phu_kstuff_flip.c
source/mman.c
source/strcasecmp.c
```

The DWARF compilation directory is:

```text
D:/dump/Claude Code/PHU Overlay/payload/shellui_overlay
```

High-value named functions include:

```text
mono_glue_init
mono_glue_resolve_scene
phu_set_fps_text_raw
phu_create_fps_label
phu_set_fps_text
phu_remove_fps_label
install_onrender_hook
OnRender_Hook
phu_check_running_on_main_thread_stub
patch_main_thread_check
```

These names and line tables provide the reconstruction map for a readable,
source-built renderer.

## Reconstruction rule

Do not reintroduce the rejected independent ShellUI design.

The next source-built hardware candidate must first reproduce the stable manual
v1.0.0 contract:

```text
PHU-r11-compatible loader contract
  + minimal Common FPS controller delta
  + minimal Common FPS renderer delta
  + full payload_args / Scene readiness / Trace Continue behavior
  + async fork entry
```

Only after manual hardware parity is restored should etaHEN autoload readiness
be added around that same initialization path.

## Local comparison tool

For developers who possess the historical donor ELF and stable Common FPS file:

```bash
python3 tools/compare_phu_r11_donor.py \
  /path/to/phu_overlay.elf \
  /path/to/Common_FPS_PS5_etaHEN_v1.0.0.plugin \
  --json donor_delta.json
```

The donor and historical release binaries are intentionally not committed to the
source tree.
