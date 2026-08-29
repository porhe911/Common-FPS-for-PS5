# Common FPS v0.28b / v1.0.0 — verified binary forensics

This document records facts recovered from the exact hardware-proven Common FPS
v0.28b payload supplied by the project owner. It is the reference contract for
the `v028b-stable-source-rebuild` branch.

## Reference binaries

etaHEN plugin:

- wrapper magic: `etaHEN_PLUGIN\0`
- title id: `CFPS00020`
- wrapper version: `0.28`
- plugin size: 1,555,173 bytes
- plugin SHA-256: `f1240511bfe3cb17a3db6f4d099cf3cd69be678d10901425a7b33911265195e1`

Embedded/standalone ELF:

- ELF size: 1,555,144 bytes
- SHA-256: `6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c`
- Build ID: `4fcee90fdb9656b3ff5026338c0846de`
- PIE, x86-64, FreeBSD ABI, not stripped

These hashes are the immutable hardware-proven reference. A source rebuild is
not called equivalent merely because it computes the same FPS value.

## Exact entry behavior

The reference `main()` is only an asynchronous entry shim:

```text
fork()
  parent (>0) -> return 0 immediately
  child (=0)  -> probe_main_entry()
  fork error (<0) -> also falls through to probe_main_entry()
```

Disassembly reference:

```text
main @ 0x27f20
  call fork
  test eax,eax
  jle child_path
  xor eax,eax
  ret
child_path:
  jmp probe_main_entry
```

This behavior is required: removing the full child initialization contract was
a known regression in historical v0.28a.

## Stable runtime architecture

```text
etaHEN / ELF loader
      |
      +-> main -> fork -> parent returns
                      |
                      +-> child: probe_main_entry
                              |
                              +-> inject_main_entry
                              |      -> locate SceShellUI
                              |      -> inject embedded renderer
                              |      -> preserve full loader payload-args contract
                              |
                              +-> FPS producer loop
                                     -> userland sysctl game PID discovery
                                     -> kernel dynlib lookup for libSceVideoOut.sprx
                                     -> DMAP process-memory reader
                                     -> VideoOut counter
                                     -> local UDP state packet to renderer
```

The stable FPS producer does not use the RC9-RC13 `FpsSampler` transport.

## Game process discovery

The reference producer uses userland `sysctl` with KERN_PROC rather than
walking kernel `allproc` for lifecycle discovery.

MIB:

```text
{ 1, 14, 8, 0 }
```

Verified `kinfo_proc` offsets on this FW family:

```text
PID  = +72  (0x48)
name = +447 (0x1bf)
```

Target process name is `eboot.bin`.

This is deliberately kept separate from the privileged DMAP translation used
only when process memory has to be read.

## VideoOut FPS counter on FW 9.60

The reference code resolves `libSceVideoOut.sprx` through kernel dynlib
metadata:

```text
kernel_dynlib_handle(pid, "libSceVideoOut.sprx", &handle)
kernel_dynlib_mapbase_addr(pid, handle)
```

FW 9.60 constants verified in the reference payload:

```text
VideoOut probe table = module_base + 0x34980
table bytes          = 0xA8
entry size           = 0x18
entry enabled        = +0x00 (u32)
entry pointer        = +0x08 (u64)
FPS counter          = root + 0x768
counter width        = 4 bytes
```

The first enabled/non-null table entry is used. The pointer stored in the table
is dereferenced once to obtain `root`.

## FPS arithmetic

The proven producer samples a monotonically increasing frame counter and wall
interval, then computes tenths of FPS as:

```text
tenths = delta_counter * 10,000,000 / elapsed_microseconds
fps    = tenths / 10.0
```

The historical producer rejects implausible values above 300.0 FPS.

## Critical difference: DMAP process reader

The hardware-proven payload contains:

```text
prw::proc_read(int pid, uintptr_t va, void* out, size_t size)
prw::translate(int pid, uintptr_t va, size_t* page_size)
```

`proc_read()` does **not** perform `PT_ATTACH`, `PT_IO`, `PT_DETACH`, or an MDBG
credential switch for every sample.

Instead:

```text
target VA
  -> walk target process page tables
  -> physical address
  -> DMAP + physical address
  -> kernel_copyout()
```

Reads crossing a page boundary are split into page-sized chunks.

### FW 9.60 offsets recovered from the reference firmware table

```text
firmware range              0x09600000 .. 0x0960ffff
allproc from KDATA          +0x2755D50
proc::pid                   +0xBC
proc::vmspace               +0x200
proc::next                  +0x00
vmspace -> pmap             +0x1D8
pmap -> PML4 virtual        +0x20
pmap -> PML4 physical       +0x28
fixed DMAP base             0xffff873b00000000
```

The fixed DMAP value is also used as the safe fallback when deriving DMAP from
PML4 virtual/physical addresses is not usable.

### Page-table walk recovered from disassembly

```text
PML4 index = (va >> 39) & 0x1ff
PDPT index = (va >> 30) & 0x1ff
PD index   = (va >> 21) & 0x1ff
PT index   = (va >> 12) & 0x1ff
```

Supported mappings:

- 1 GiB page (`PS` bit in PDPTE)
- 2 MiB page (`PS` bit in PDE)
- 4 KiB page (PTE)

Physical masks recovered from the reference:

```text
4 KiB: 0x000ffffffffff000
2 MiB: 0x000fffffffe00000
1 GiB: 0x000fffffc0000000
```

This DMAP transport is the primary implementation target of the source rebuild.

## Renderer/bootstrap facts

`probe_main_entry()` first calls `inject_main_entry()`.

The reference ELF embeds a renderer blob with symbols:

```text
_binary_libphu_overlay_so_start
_binary_libphu_overlay_so_end
_binary_libphu_overlay_so_size
```

The historical renderer lineage is PHU Games Tools v1.14.25-r11. The source
rebuild must not silently replace this loader/runtime contract with the failed
RC source-only ShellUI design.

Until a source-equivalent renderer bootstrap is proven, renderer reconstruction
is a separate milestone from the FPS producer reconstruction.

## Source lineage visible in the reference ELF

The non-stripped reference contains FILE/DWARF names including:

```text
main.cpp
injector.c
elfldr.c
hijacker.cpp
proc.c
proc_rw_v940.cpp
offsets_v940.cpp
ucred.c
pt.c
mdbg.c
hook_vout.cpp
shm_writer.cpp
videoout_ioctl.cpp
videoout_dcb_ring.cpp
videoout_dcb_global.cpp
phu_recovery.cpp
```

A DWARF compile directory present in the reference is:

```text
D:/dump/Claude Code/PHU Overlay/payload/probe
```

These names establish binary lineage; they are not by themselves corresponding
source files.

## Rebuild rules

1. Keep `main` untouched until the rebuild passes hardware lifecycle tests.
2. Do not copy RC9-RC13 remote-read architecture into this branch.
3. No periodic ptrace or MDBG sampling.
4. Preserve asynchronous fork entry.
5. Preserve userland sysctl lifecycle discovery.
6. Preserve kernel dynlib VideoOut lookup.
7. Rebuild DMAP `proc_read()` first and verify it in CI before hardware use.
8. Treat renderer/bootstrap as a separate parity milestone.
9. Do not call a build v1.0.0-equivalent until repeated PS4/PS5 game switching
   passes without KP.
