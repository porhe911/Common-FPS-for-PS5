# Historical v1.0.0 binary forensics

This document records facts recovered from the hardware-stable Common FPS
v1.0.0 release. It exists to guide a source reconstruction that matches the
real working architecture instead of inventing a new renderer path.

## Golden hashes

Public etaHEN plugin:

```text
Common_FPS_PS5_etaHEN_v1.0.0.plugin
SHA-256 f1240511bfe3cb17a3db6f4d099cf3cd69be678d10901425a7b33911265195e1
```

The etaHEN wrapper is 29 bytes:

```text
etaHEN_PLUGIN\0CFPS00020\00.28\0
```

The embedded controller payload is therefore the historical internal v0.28b
ELF:

```text
SHA-256 6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c
```

The stable ELF is not stripped. It retains `.symtab` and enough compilation
unit names to recover the original architecture.

## Recovered project compilation units

The stable controller contains `STT_FILE` records for the following
project-specific units in addition to SDK/libc runtime objects:

```text
dbg.cpp
hijacker.cpp
kernel.cpp
notify.cpp
offsets_v940.cpp
main.cpp
hook_vout.cpp
proc_rw_v940.cpp
shm_writer.cpp
phu_patcher.cpp
phu_cheat_engine.cpp
phu_menu_shm.cpp
phu_recovery.cpp
videoout_ioctl.cpp
videoout_dcb_ring.cpp
videoout_dcb_global.cpp
elfldr.c
injector.c
notify.c
proc.c
pt.c
ucred.c
phu_notify.c
phu_config.c
phu_stats.c
phu_fw_offsets.c
phu_kstuff_flip.c
phu_kstuff_ctrl.c
phu_klog_mirror.c
phu_patch_types.c
phu_xml_parser.c
phu_json_parser.c
phu_shn_parser.c
phu_mc4_parser.c
phu_mc4_aes.c
phu_mc4_base64.c
phu_appver.c
```

This proves that the stable release was built from a PHU-derived payload/probe
source tree plus the Common FPS-specific patches. The current clean C++ rewrite
must therefore not be described as byte-for-byte corresponding source for the
historical v1.0.0 binary.

## Recovered high-value symbols

The stable ELF retains symbols for the controller and loader path, including:

```text
main
probe_main_entry
inject_main_entry
inject_elf
elfldr_load
elfldr_payload_args
Hijacker::getEboot()
videoout_ioctl::init()
videoout_ioctl::sample()
phu_recovery::check_and_reinject()
```

The exact historical v0.28b contract remains the reconstruction target:

```text
etaHEN Run / standalone ELF
        -> fork()
        -> parent returns immediately
        -> child performs the full stable initialization
        -> locate/wait for SceShellUI
        -> one Trace-Continue loader session
        -> load embedded renderer
        -> full elfldr_payload_args()
        -> renderer Scene readiness
        -> real read-only VideoOut FPS lifecycle
```

Do not remove `elfldr_payload_args()`. Historical v0.28a did so and was rejected
on hardware after a game-launch kernel panic.

## Exact embedded renderer recovered

The stable controller exposes binary-boundary symbols:

```text
_binary_libphu_overlay_so_start
_binary_libphu_overlay_so_end
```

The range is 260,248 bytes and hashes to:

```text
libphu_overlay.so
SHA-256 3640cbcaf907bc900e257503b3c8922fdbc129c492a76a2348fd75c84f68b91c
```

This renderer is a separate ELF shared object embedded in the controller. It is
not committed to the open-source tree. `tools/analyze_v1_stable.py` can extract
it locally from a user's verified historical v1.0.0 binary for forensic
comparison.

Relevant strings and imports show the stable renderer uses the real PS5
ShellUI/PUI path, including:

```text
Sce.PlayStation.PUI.dll
Sce.Vsh.ShellUI.AppSystem.dll
FindContainerSceneByPath
Application
Update
RootWidget
FPS:
FPS: loading
/system_tmp/phu_hook_ipc
/user/data/PHU/phu_overlay_online
```

It also contains its own detour/IPC machinery. This matters because the later
experimental Stage B/B2/B3 source rewrites attempted a much simpler second
`Application.Update` hook and were rejected on hardware. The source
reconstruction must model the stable renderer's coexistence contract rather
than repeating that design.

## Hardware-rejected paths

The following experimental source branches are explicitly not parity targets:

- Stage B: second ordinary `Application.Update` detour;
- Stage B2: chaining another Common FPS hook over etaHEN's live hook;
- Stage B3: stronger visual probe on the same chained-hook architecture.

Observed regressions included no Common FPS overlay, etaHEN GPU/CPU/RAM being
the only visible overlay, kernel panic on Stop/Run, more frequent YBJB kernel
panics, and loss of normal CUSA/PPSA display after jailbreak activation.

Therefore the reconstruction rule is:

> Preserve the v1.0.0 loader/renderer contract first. Add autoload readiness
> only after a source-built manual-run version has hardware parity.

## Reconstruction milestones

1. **Forensic map** — verify the historical binary, recover module/symbol map,
   and document exact invariants.
2. **Controller parity** — rebuild the read-only game detection, VideoOut
   sampler, lifecycle, Trace-Continue loader and full payload-args sequence.
3. **Renderer parity** — reconstruct the minimal stable ShellUI/PUI renderer
   behavior and its safe IPC/detour contract.
4. **Manual hardware parity** — PS4 + PS5, real integer FPS, bottom-left,
   font 26, purple label/white value, Game A -> Home -> Game B, no KP.
5. **Autoload-only change** — add a conservative readiness gate before the
   already-proven stable initialization. Do not redesign the renderer.
6. **Cross-firmware diagnostics** — only after FW 9.60 parity, investigate the
   FW 7.60 tester's permanent `FPS: loading` separately from renderer startup.

## Forensic tool

Run against a local copy of the known release; the release binary is not
required in the repository:

```bash
python3 tools/analyze_v1_stable.py Common_FPS_PS5_etaHEN_v1.0.0.plugin \
  --extract-renderer out/libphu_overlay.so \
  --json out/v1_forensics.json
```

The tool validates the known wrapper/payload hashes, lists ELF `STT_FILE`
records, records high-value symbols and verifies the recovered renderer hash.
It never runs or patches the payload.
