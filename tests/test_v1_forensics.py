#!/usr/bin/env python3
"""Small host tests for tools/analyze_v1_stable.py.

The historical stable binary is intentionally not stored in the source tree,
so CI tests only the format/parser helpers with synthetic data.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.analyze_v1_stable import (  # noqa: E402
    PLUGIN_HEADER_SIZE,
    PLUGIN_MAGIC,
    Symbol,
    unique_file_symbols,
    unwrap_plugin,
)


def test_plugin_wrapper() -> None:
    header = b"etaHEN_PLUGIN\x00CFPS00020\x000.28\x00"
    assert len(header) == PLUGIN_HEADER_SIZE
    payload = b"\x7fELF" + b"test-payload"
    elf, meta = unwrap_plugin(header + payload)
    assert elf == payload
    assert meta == {
        "marker": "etaHEN_PLUGIN",
        "title_id": "CFPS00020",
        "version": "0.28",
    }


def test_plain_elf_passthrough() -> None:
    payload = b"\x7fELF" + b"standalone"
    elf, meta = unwrap_plugin(payload)
    assert elf == payload
    assert meta is None


def test_file_symbol_dedup() -> None:
    stt_file = 4
    symbols = [
        Symbol("main.cpp", stt_file, 1, 0, 0),
        Symbol("main.cpp", stt_file, 1, 0, 0),
        Symbol("elfldr.c", stt_file, 1, 0, 0),
        Symbol("ordinary_function", 2, 1, 0, 0),
    ]
    assert unique_file_symbols(symbols) == ["main.cpp", "elfldr.c"]


def test_reject_non_elf_plugin_payload() -> None:
    header = PLUGIN_MAGIC + b"CFPS00020\x000.28\x00"
    assert len(header) == PLUGIN_HEADER_SIZE
    try:
        unwrap_plugin(header + b"NOPE")
    except ValueError as exc:
        assert "not ELF" in str(exc)
    else:
        raise AssertionError("malformed plugin payload was accepted")


def main() -> int:
    test_plugin_wrapper()
    test_plain_elf_passthrough()
    test_file_symbol_dedup()
    test_reject_non_elf_plugin_payload()
    print("v1 forensic helper tests: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
