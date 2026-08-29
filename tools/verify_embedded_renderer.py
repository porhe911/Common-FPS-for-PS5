#!/usr/bin/env python3
"""Verify that both public launch forms carry the exact ShellUI ELF bytes."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--controller", required=True, type=Path)
    parser.add_argument("--plugin", required=True, type=Path)
    parser.add_argument("--renderer", required=True, type=Path)
    args = parser.parse_args()

    controller = args.controller.read_bytes()
    plugin = args.plugin.read_bytes()
    renderer = args.renderer.read_bytes()

    if renderer[:4] != b"\x7fELF":
        raise SystemExit("renderer is not an ELF")
    if controller[:4] != b"\x7fELF":
        raise SystemExit("controller is not an ELF")
    if renderer not in controller:
        raise SystemExit("exact ShellUI ELF byte sequence not found in controller")
    if controller not in plugin:
        raise SystemExit("controller ELF not found in etaHEN plugin wrapper")
    if renderer not in plugin:
        raise SystemExit("embedded ShellUI ELF not found in etaHEN plugin")

    print(f"PASS embedded renderer: {len(renderer)} bytes")
    print(f"PASS standalone ELF:    {len(controller)} bytes")
    print(f"PASS etaHEN plugin:     {len(plugin)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
