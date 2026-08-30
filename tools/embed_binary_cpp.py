#!/usr/bin/env python3
import argparse
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("input")
p.add_argument("output")
p.add_argument("symbol")
a = p.parse_args()

data = Path(a.input).read_bytes()
out = Path(a.output)
out.parent.mkdir(parents=True, exist_ok=True)

with out.open("w", encoding="utf-8", newline="\n") as f:
    f.write("#include <cstddef>\n\nextern \"C\" {\n")
    f.write(f"extern const unsigned char {a.symbol}[] = {{\n")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        f.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
    f.write("};\n")
    f.write(f"extern const std::size_t {a.symbol}_size = sizeof({a.symbol});\n")
    f.write("}\n")
