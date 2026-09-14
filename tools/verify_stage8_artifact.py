#!/usr/bin/env python3
"""Verify the Stage 8 universal diagnostic ELF/plugin boundary."""

from __future__ import annotations

import hashlib
import pathlib
import sys


PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00050\0" + b"1.48\0"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: verify_stage8_artifact.py stage8.elf stage8.plugin",
            file=sys.stderr,
        )
        return 2

    elf_path = pathlib.Path(sys.argv[1])
    plugin_path = pathlib.Path(sys.argv[2])
    elf = elf_path.read_bytes()
    plugin = plugin_path.read_bytes()

    renderer_offset = elf.find(b"\x7fELF", 1)
    renderer = elf[renderer_offset:] if renderer_offset >= 0 else b""

    checks = [
        (elf.startswith(b"\x7fELF"), "controller is not an ELF"),
        (plugin.startswith(PLUGIN_HEADER), "plugin metadata mismatch"),
        (plugin[len(PLUGIN_HEADER):] == elf, "plugin body differs from ELF"),
        (renderer_offset > 0, "embedded renderer ELF missing"),
        (len(renderer) > 4096, "embedded renderer ELF is unexpectedly small"),
        (
            b"Common FPS Universal Stage 8 self-hook dynamic VideoOut scan"
            in elf,
            "Stage 8 runtime marker missing",
        ),
        (b"internal_fork=absent" in elf, "no-fork marker missing"),
        (
            b"renderer=shared_elf_atomic_selfhook" in elf,
            "atomic self-hook marker missing",
        ),
        (b"stability_gate=10" in elf, "startup stability gate missing"),
        (b"sampler=videoout_dynamic_1s" in elf, "dynamic sampler marker missing"),
        (b"read=mdbg" in elf, "MDBG read marker missing"),
        (
            b"/data/CommonFPS_universal_stage8.log" in elf,
            "controller diagnostic log path missing",
        ),
        (
            b"/data/CommonFPS_universal_stage8_shellui.log" in renderer,
            "renderer diagnostic log path missing",
        ),
        (b"Application.Update hook online" in renderer, "hook code missing"),
        (b"native-selfhook" in renderer, "native hook mode missing"),
        (b"id_commonfps_value" in renderer, "PUI renderer missing"),
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
    print(f"embedded renderer offset=0x{renderer_offset:x} size={len(renderer)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
