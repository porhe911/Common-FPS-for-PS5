#!/usr/bin/env python3
"""Verify the byte-exact PARITY TEST20 ELF and etaHEN plugin."""

from __future__ import annotations

import hashlib
import pathlib
import sys


EXPECTED_ELF_SHA256 = (
    "b791baf6f85063d0c7ec57eebcbf60116dff58dfc4ec74aa6d9dedcc1ecc32ef"
)
EXPECTED_PLUGIN_SHA256 = (
    "7bdcc16cc582bd89c7f1680471436ef58fdc53bab159c7e36278a5e8f1c8fa49"
)
PLUGIN_HEADER = b"etaHEN_PLUGIN\0CFPS00039\0" + b"1.28\0"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} TEST20.elf TEST20.plugin", file=sys.stderr)
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
        (b"PARITY TEST20 sampler-only no-recorder A/B" in elf,
         "TEST20 marker missing"),
        (b"renderer=disabled" in elf, "renderer-disabled marker missing"),
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
