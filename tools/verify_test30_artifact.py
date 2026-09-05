#!/usr/bin/env python3
"""Verify PARITY TEST30 metadata, payload identity and test boundary."""

from __future__ import annotations

import hashlib
import pathlib
import sys


PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00049\0" + b"1.38\0"
EXPECTED_ELF_SHA256 = (
    "732db78e9329fcabbe93a66972e5d20e5137256769385138348cc88c45a99f9a"
)
EXPECTED_PLUGIN_SHA256 = (
    "b766d1c479cf5720b723fa73caab442bece4048420ed371a70ba6378b49b9515"
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} TEST30.elf TEST30.plugin", file=sys.stderr)
        return 2

    elf_path = pathlib.Path(sys.argv[1])
    plugin_path = pathlib.Path(sys.argv[2])
    elf = elf_path.read_bytes()
    plugin = plugin_path.read_bytes()

    checks = [
        (digest(elf) == EXPECTED_ELF_SHA256, "ELF SHA-256 mismatch"),
        (digest(plugin) == EXPECTED_PLUGIN_SHA256, "plugin SHA-256 mismatch"),
        (plugin.startswith(PLUGIN_HEADER), "plugin metadata mismatch"),
        (plugin[len(PLUGIN_HEADER):] == elf, "plugin body differs from ELF"),
        (b"PARITY TEST30 tracked-process no-fork A/B" in elf,
         "TEST30 marker missing"),
        (b"internal_fork=absent" in elf, "no-fork marker missing"),
        (b"renderer=disabled" in elf, "renderer-disabled marker missing"),
        (b"CommonFPS_v110_test30_tracked_process.log" in elf,
         "TEST30 log path missing"),
        (b"PARITY TEST20 sampler-only no-recorder A/B" not in elf,
         "stale TEST20 runtime marker present"),
    ]

    failures = [message for ok, message in checks if not ok]
    if failures:
        for message in failures:
            print(f"ERROR: {message}", file=sys.stderr)
        return 1

    print(f"verified ELF    {EXPECTED_ELF_SHA256}")
    print(f"verified plugin {EXPECTED_PLUGIN_SHA256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
