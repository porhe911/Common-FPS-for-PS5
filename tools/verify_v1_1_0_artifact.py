#!/usr/bin/env python3
"""Verify the v1.1.0 metadata and tracked-renderer boundary."""

from __future__ import annotations

import hashlib
import pathlib
import sys


PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00050\0" + b"1.39\0"

# Reference hashes from the reproducible PS5 source build. Keeping the values
# here makes later local/CI builds fail closed if the source or toolchain drifts.
EXPECTED_ELF_SHA256 = (
    "4f544fa00f7a430e64c4c8d0ed42d0463d2370c81dabd9141599c27c4f3f99d6"
)
EXPECTED_PLUGIN_SHA256 = (
    "39333081ecd93ade60d1b75fb0032a1e996fcf17ad47a9adfc0290591596e44e"
)
EXPECTED_RENDERER_SHA256 = (
    "7880aec891cb95cc860753d5a3fed1dfbb23caf526b6106275c3b2cc02b8e465"
)
RENDERER_SIZE = 70968


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: verify_v1_1_0_artifact.py v1.1.0.elf v1.1.0.plugin",
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
        (
            elf_sha == EXPECTED_ELF_SHA256,
            "ELF SHA-256 mismatch",
        ),
        (
            plugin_sha == EXPECTED_PLUGIN_SHA256,
            "plugin SHA-256 mismatch",
        ),
        (plugin.startswith(PLUGIN_HEADER), "plugin metadata mismatch"),
        (plugin[len(PLUGIN_HEADER) :] == elf, "plugin body differs from ELF"),
        (
            b"Common FPS v1.1.0 tracked-process renderer\n" in elf,
            "v1.1.0 runtime marker missing",
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
            b"CommonFPS_v110.log" in elf,
            "v1.1.0 log path missing",
        ),
        (b"id_commonfps_value" in elf, "embedded renderer missing"),
        (
            len(renderer) == RENDERER_SIZE
            and digest(renderer) == EXPECTED_RENDERER_SHA256,
            "embedded renderer hash mismatch",
        ),
        (
            b"PARITY TEST30 tracked-process no-fork A/B" not in elf,
            "stale TEST30 runtime marker present",
        ),
        (
            b"PARITY TEST20 sampler-only no-recorder A/B" not in elf,
            "stale TEST20 runtime marker present",
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
