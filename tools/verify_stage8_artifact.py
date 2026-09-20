#!/usr/bin/env python3
"""Verify the Stage 8.2 universal diagnostic ELF/plugin boundary."""

from __future__ import annotations

import hashlib
import pathlib
import sys


PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00050\0" + b"1.50\0"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "usage: verify_stage8_artifact.py "
            "stage8_2.elf stage8_2.plugin stage8_2_renderer.elf",
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
            b"Common FPS Universal Stage 8.2 stopped MDBG hook + indirect scan"
            in elf,
            "Stage 8.2 runtime marker missing",
        ),
        (b"internal_fork=absent" in elf, "no-fork marker missing"),
        (
            b"renderer=shared_elf_stopped_mdbg_hook" in elf,
            "stopped MDBG hook marker missing",
        ),
        (b"stability_gate=10" in elf, "startup stability gate missing"),
        (
            b"sampler=videoout_indirect_dynamic_1s" in elf,
            "indirect sampler marker missing",
        ),
        (b"read=mdbg" in elf, "MDBG read marker missing"),
        (
            b"/data/CommonFPS_universal_stage8_2.log" in elf,
            "controller diagnostic log path missing",
        ),
        (
            b"/data/CommonFPS_universal_stage8_2_shellui.log" in renderer,
            "renderer diagnostic log path missing",
        ),
        (b"Application.Update hook online" in renderer, "hook code missing"),
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
            b"commonfps_stage8_2_hook_request.bin" in renderer,
            "renderer hook request protocol missing",
        ),
        (
            b"mdbg_copyin" in elf,
            "controller MDBG patch primitive missing",
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
