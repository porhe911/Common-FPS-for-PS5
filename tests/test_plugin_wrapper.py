#!/usr/bin/env python3
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = root/"tools/make_etahen_plugin.py"

spec = importlib.util.spec_from_file_location("packer", p)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

elf = b"\x7fELF" + bytes(range(32))
out = m.build_plugin(elf, "CFPS00021", "1.10")

header = b"etaHEN_PLUGIN\x00CFPS00021\x001.10\x00"
assert out[:len(header)] == header
assert out[len(header):] == elf
print("etaHEN wrapper test: PASS")
