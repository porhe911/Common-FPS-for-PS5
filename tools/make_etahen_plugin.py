#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
from pathlib import Path

MAGIC = b"etaHEN_PLUGIN\x00"

def build_plugin(elf: bytes, title_id: str, version: str) -> bytes:
    if len(title_id) != 9:
        raise ValueError("title id must be 9 characters")
    return (
        MAGIC
        + title_id.encode("ascii") + b"\x00"
        + version.encode("ascii") + b"\x00"
        + elf
    )

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf", type=Path)
    ap.add_argument("plugin", type=Path)
    ap.add_argument("--title-id", default="CFPS00021")
    ap.add_argument("--version", default="1.10")
    a = ap.parse_args()
    a.plugin.write_bytes(build_plugin(
        a.elf.read_bytes(), a.title_id, a.version
    ))

if __name__ == "__main__":
    main()
