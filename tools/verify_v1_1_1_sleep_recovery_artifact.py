#!/usr/bin/env python3
"""Verify the v1.1.1 ShellUI sleep-recovery candidate."""

from __future__ import annotations

import hashlib
import pathlib
import sys


PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00050\0" + b"1.39\0"
RENDERER_SIZE = 70968


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: verify_v1_1_1_sleep_recovery_artifact.py "
            "v1.1.1.elf v1.1.1.plugin",
            file=sys.stderr,
        )
        return 2

    elf_path = pathlib.Path(sys.argv[1])
    plugin_path = pathlib.Path(sys.argv[2])
    elf = elf_path.read_bytes()
    plugin = plugin_path.read_bytes()
    elf_sha = digest(elf)
    plugin_sha = digest(plugin)
    renderer_offset = elf.find(b"\x7fELF", 1)
    renderer = (
        elf[renderer_offset : renderer_offset + RENDERER_SIZE]
        if renderer_offset >= 0
        else b""
    )
    controller_image = elf[:renderer_offset] if renderer_offset >= 0 else elf

    checks = [
        (plugin.startswith(PLUGIN_HEADER), "plugin metadata mismatch"),
        (plugin[len(PLUGIN_HEADER) :] == elf, "plugin body differs from ELF"),
        (
            b"Common FPS v1.1.1 sleep-recovery\n" in elf,
            "v1.1.1 runtime marker missing",
        ),
        (b"internal_fork=absent" in elf, "no-fork marker missing"),
        (
            b"renderer=shared_elf_etaHEN_update_hook" in elf,
            "renderer marker missing",
        ),
        (b"injection=target_stack_pthread" in elf, "bootstrap marker missing"),
        (b"ipc=udp_loopback_1s" in elf, "IPC marker missing"),
        (b"mono_gc=pinned" in elf, "GC marker missing"),
        (
            b"CommonFPS_v111_sleep_recovery.log" in elf,
            "sleep-recovery log path missing",
        ),
        (
            b"ShellUI lifecycle previous_pid=" in elf
            and b"renderer_reset=1" in elf,
            "ShellUI lifecycle reset marker missing",
        ),
        (b"id_commonfps_value" in elf, "embedded renderer missing"),
        (
            len(renderer) == RENDERER_SIZE,
            "embedded renderer size mismatch",
        ),
        (
            b"PARITY TEST" not in controller_image,
            "stale parity-test controller marker present",
        ),
    ]

    failures = [message for ok, message in checks if not ok]
    if failures:
        for message in failures:
            print(f"ERROR: {message}", file=sys.stderr)
        return 1

    print(f"verified ELF    {elf_sha}")
    print(f"verified plugin {plugin_sha}")
    print(f"verified renderer {digest(renderer)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
