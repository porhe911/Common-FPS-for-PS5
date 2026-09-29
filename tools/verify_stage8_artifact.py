#!/usr/bin/env python3
"""Verify the Common FPS for PS5 v1.2.2 release boundary."""

from __future__ import annotations

import hashlib
import pathlib
import struct
import sys


PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00059\0" + b"1.22\0"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def trampoline_is_writable_executable(data: bytes) -> bool:
    """The native trampoline is self-written only in our own ELF mapping."""
    try:
        phoff, shoff = struct.unpack_from("<QQ", data, 32)
        phentsize, phnum, shentsize, shnum = struct.unpack_from("<HHHH", data, 54)
        if phentsize != 56 or shentsize != 64:
            return False
        sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * 64)
                    for i in range(shnum)]
        for section in sections:
            if section[1] != 2 or section[9] != 24:  # SHT_SYMTAB
                continue
            strings = sections[section[6]]
            names = data[strings[4]:strings[4] + strings[5]]
            for offset in range(section[4], section[4] + section[5], 24):
                name, _, _, _, value, size = struct.unpack_from("<IBBHQQ", data, offset)
                if names[name:].split(b"\0", 1)[0] != b"commonfps_update_trampoline":
                    continue
                if size < 128:
                    return False
                for i in range(phnum):
                    segment = struct.unpack_from("<IIQQQQQQ", data, phoff + i * 56)
                    if (segment[0] == 1 and segment[1] & 3 == 3 and
                            segment[3] <= value and
                            value + 128 <= segment[3] + segment[6]):
                        return True
        return False
    except (struct.error, IndexError):
        return False


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "usage: verify_stage8_artifact.py "
            "common_fps.elf common_fps.plugin shellui_renderer.elf",
            file=sys.stderr,
        )
        return 2

    elf_path = pathlib.Path(sys.argv[1])
    plugin_path = pathlib.Path(sys.argv[2])
    renderer_path = pathlib.Path(sys.argv[3])

    elf = elf_path.read_bytes()
    plugin = plugin_path.read_bytes()
    renderer = renderer_path.read_bytes()
    renderer_offset = elf.find(renderer)

    checks = [
        (elf.startswith(b"\x7fELF"), "controller is not an ELF"),
        (renderer.startswith(b"\x7fELF"), "renderer is not an ELF"),
        (plugin.startswith(PLUGIN_HEADER), "plugin metadata mismatch"),
        (plugin[len(PLUGIN_HEADER):] == elf, "plugin body differs from ELF"),
        (renderer_offset > 0, "exact renderer ELF is not embedded"),
        (len(renderer) > 4096, "renderer ELF is unexpectedly small"),
        (
            b"Common FPS for PS5 v1.2.2"
            in elf,
            "v1.2.2 runtime marker missing",
        ),
        (b"internal_fork=absent" in elf, "no-fork marker missing"),
        (b"CommonFPS.elf" in elf, "Payload Manager process name marker missing"),
        (b"sceKernelSetProcessName" in elf, "process-name API import missing"),
        (
            b"renderer=shared_elf_stopped_chain_hook" in elf,
            "stopped MDBG hook marker missing",
        ),
        (b"stability_gate=10" in elf, "startup stability gate missing"),
        (
            b"game_gate=process_present_stable_3s" in elf,
            "stable game-process injection gate missing",
        ),
        (
            b"renderer_injection=deferred_until_game" in elf,
            "deferred renderer injection marker missing",
        ),
        (
            b"sampler=videoout_preferred+dce_auth_adaptive standby=60s_reprobe" in elf,
            "DCE standby sampler marker missing",
        ),
        (
            b"Sampler policy pid=%d state=%s backend=%s" in elf,
            "sampler standby policy logging missing",
        ),
        (
            b"dce-miss-wake" in elf,
            "DCE fallback wake path missing",
        ),
        (
            b"Fallback sampler online" in elf,
            "fallback sampler code missing",
        ),
        (
            b"privileged-window" in elf,
            "DCE auth-window open path missing",
        ),
        (
            b"DCE adaptive counter selected" in elf,
            "adaptive DCE counter discovery missing",
        ),
        (
            b"/system_tmp/fps_sample" in elf,
            "shared HEN FPS fallback missing",
        ),
        (
            b"/dev/dce" in elf,
            "DCE device path missing",
        ),
        (
            b"highfw_guard=ptrace_io_1byte" in elf,
            "high firmware guard marker missing",
        ),
        (b"read=mdbg" in elf, "MDBG read marker missing"),
        (
            b"/data/CommonFPS_v1_2_2.log" in elf,
            "controller diagnostic log path missing",
        ),
        (
            b"/data/CommonFPS_v1_2_2_shellui.log" in renderer,
            "renderer diagnostic log path missing",
        ),
        (
            b"legacy main-thread guard online" in renderer,
            "legacy stopped UI-thread guard path missing",
        ),
        (
            b"renderer backend selected mode=legacy-background-pui" in renderer,
            "legacy background PUI backend missing",
        ),
        (
            b"CheckRunningOnMainThread" in renderer,
            "legacy UI-thread guard lookup missing",
        ),
        (b"Application.Update hook online" in renderer, "hook code missing"),
        (b"etahen-stopped-chain" in renderer, "stopped etaHEN chain missing"),
        (b"stopped_patch=0" not in renderer, "live method write path remains"),
        (b"method_writes=controller_only" in elf, "controller-only write marker missing"),
        (b"last_stage=%s quarantine=1" in elf, "failure quarantine missing"),
        (trampoline_is_writable_executable(renderer), "own trampoline is not in RWX memory"),
        (
            b"mono_mprotect" not in renderer,
            "unsafe live page-protection import is still present",
        ),
        (
            b"kernel_mprotect" not in renderer,
            "unsafe SDK kernel_mprotect import is still present",
        ),
        (
            b"native-stopped-mdbg" in renderer,
            "stopped native hook mode missing",
        ),
        (b"id_commonfps_value" in renderer, "PUI renderer missing"),
        (
            b"commonfps_v122_hook_request.bin" in renderer,
            "renderer hook request protocol missing",
        ),
        (
            b"mdbg_copyin" in elf,
            "controller MDBG patch primitive missing",
        ),
        (
            b"mdbg_copyout" not in renderer,
            "renderer must not import controller-only mdbg_copyout",
        ),
        (
            b"hook_probe_request" in renderer,
            "controller probe request missing",
        ),
        (
            b"native-selfhook" not in renderer,
            "unsafe native self-hook path is still present",
        ),
        (
            b"PARITY TEST13 target-thread bootstrap stack" not in renderer,
            "stale Stage 7 renderer marker present",
        ),
    ]

    failures = [message for ok, message in checks if not ok]
    if failures:
        for message in failures:
            print(f"ERROR: {message}", file=sys.stderr)
        return 1

    print(f"verified ELF      {digest(elf)}")
    print(f"verified plugin   {digest(plugin)}")
    print(f"verified renderer {digest(renderer)}")
    print(
        f"embedded renderer offset=0x{renderer_offset:x} "
        f"size={len(renderer)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
