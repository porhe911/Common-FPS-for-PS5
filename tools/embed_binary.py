#!/usr/bin/env python3
"""Embed a binary file as a C++ byte array for the PS5 controller."""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} INPUT OUTPUT.cpp", file=sys.stderr)
        return 2

    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    data = src.read_bytes()

    dst.parent.mkdir(parents=True, exist_ok=True)
    with dst.open("w", encoding="utf-8", newline="\n") as out:
        out.write("#include <cstddef>\n#include <cstdint>\n\n")
        out.write("alignas(16) const std::uint8_t commonfps_shellui_elf[] = {\n")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            out.write("    ")
            out.write(", ".join(f"0x{b:02x}" for b in chunk))
            out.write(",\n")
        out.write("};\n\n")
        out.write(
            f"const std::size_t commonfps_shellui_elf_size = {len(data)}u;\n"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
