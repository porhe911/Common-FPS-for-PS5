#!/usr/bin/env python3
"""Forensic verifier for the historical Common FPS v1.0.0 stable binary.

This tool does not patch or execute the payload. It validates the known stable
etaHEN wrapper / ELF hashes, inventories ELF FILE symbols, and extracts the
embedded ShellUI renderer by symbol boundaries when present.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

PLUGIN_MAGIC = b"etaHEN_PLUGIN\x00"
PLUGIN_HEADER_SIZE = 29
KNOWN_PLUGIN_SHA256 = "f1240511bfe3cb17a3db6f4d099cf3cd69be678d10901425a7b33911265195e1"
KNOWN_ELF_SHA256 = "6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c"
KNOWN_RENDERER_SHA256 = "3640cbcaf907bc900e257503b3c8922fdbc129c492a76a2348fd75c84f68b91c"
RENDERER_START = "_binary_libphu_overlay_so_start"
RENDERER_END = "_binary_libphu_overlay_so_end"
HIGH_VALUE_SYMBOLS = (
    "main",
    "probe_main_entry",
    "inject_main_entry",
    "inject_elf",
    "elfldr_load",
    "elfldr_payload_args",
    "_ZN8Hijacker8getEbootEv",
    "_ZN12videoout_ioctl4initEv",
    "_ZN12videoout_ioctl6sampleEv",
    "_ZN13phu_recovery18check_and_reinjectEv",
    RENDERER_START,
    RENDERER_END,
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


@dataclass(frozen=True)
class Section:
    index: int
    name: str
    sh_type: int
    flags: int
    addr: int
    offset: int
    size: int
    link: int
    entsize: int


@dataclass(frozen=True)
class Symbol:
    name: str
    info: int
    shndx: int
    value: int
    size: int

    @property
    def st_type(self) -> int:
        return self.info & 0x0F


class Elf64:
    def __init__(self, data: bytes):
        self.data = data
        if len(data) < 0x40 or data[:4] != b"\x7fELF":
            raise ValueError("not an ELF file")
        if data[4] != 2 or data[5] != 1:
            raise ValueError("expected ELF64 little-endian")
        self.e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
        self.e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
        self.e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
        self.e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
        if self.e_shentsize < 64:
            raise ValueError("unexpected section header size")
        self.sections = self._parse_sections()
        self.symbols = self._parse_symbols()

    def _raw_shdr(self, index: int):
        off = self.e_shoff + index * self.e_shentsize
        if off + 64 > len(self.data):
            raise ValueError("section table extends past end of file")
        return struct.unpack_from("<IIQQQQIIQQ", self.data, off)

    def _parse_sections(self) -> list[Section]:
        raw = [self._raw_shdr(i) for i in range(self.e_shnum)]
        if self.e_shstrndx >= len(raw):
            raise ValueError("invalid shstrndx")
        names_hdr = raw[self.e_shstrndx]
        names = self.data[names_hdr[4]: names_hdr[4] + names_hdr[5]]

        def cstr(blob: bytes, off: int) -> str:
            if off >= len(blob):
                return ""
            end = blob.find(b"\x00", off)
            if end < 0:
                end = len(blob)
            return blob[off:end].decode("utf-8", "replace")

        out: list[Section] = []
        for i, h in enumerate(raw):
            out.append(Section(
                index=i,
                name=cstr(names, h[0]),
                sh_type=h[1],
                flags=h[2],
                addr=h[3],
                offset=h[4],
                size=h[5],
                link=h[6],
                entsize=h[9],
            ))
        return out

    def _parse_symbols(self) -> list[Symbol]:
        result: list[Symbol] = []
        for sec in self.sections:
            if sec.sh_type not in (2, 11):  # SHT_SYMTAB / SHT_DYNSYM
                continue
            if not sec.entsize or sec.link >= len(self.sections):
                continue
            strings_sec = self.sections[sec.link]
            strings = self.data[strings_sec.offset: strings_sec.offset + strings_sec.size]
            count = sec.size // sec.entsize
            for i in range(count):
                off = sec.offset + i * sec.entsize
                if off + 24 > len(self.data):
                    break
                st_name, st_info, _st_other, st_shndx, st_value, st_size = struct.unpack_from(
                    "<IBBHQQ", self.data, off
                )
                if st_name >= len(strings):
                    name = ""
                else:
                    end = strings.find(b"\x00", st_name)
                    if end < 0:
                        end = len(strings)
                    name = strings[st_name:end].decode("utf-8", "replace")
                result.append(Symbol(name, st_info, st_shndx, st_value, st_size))
        return result

    def symbols_named(self, name: str) -> list[Symbol]:
        return [s for s in self.symbols if s.name == name]

    def va_to_file_offset(self, value: int) -> int:
        # Prefer a physical section rather than SHT_NOBITS storage.
        for sec in self.sections:
            if sec.sh_type == 8:  # SHT_NOBITS
                continue
            if sec.size and sec.addr <= value < sec.addr + sec.size:
                delta = value - sec.addr
                file_off = sec.offset + delta
                if file_off < len(self.data):
                    return file_off
        raise ValueError(f"cannot map VA 0x{value:x} to file offset")


def unwrap_plugin(data: bytes) -> tuple[bytes, dict[str, str] | None]:
    if not data.startswith(PLUGIN_MAGIC):
        return data, None
    if len(data) < PLUGIN_HEADER_SIZE + 4:
        raise ValueError("etaHEN plugin is truncated")
    header = data[:PLUGIN_HEADER_SIZE]
    parts = header.split(b"\x00")
    if len(parts) < 4:
        raise ValueError("etaHEN plugin header is malformed")
    meta = {
        "marker": parts[0].decode("ascii", "replace"),
        "title_id": parts[1].decode("ascii", "replace"),
        "version": parts[2].decode("ascii", "replace"),
    }
    elf = data[PLUGIN_HEADER_SIZE:]
    if not elf.startswith(b"\x7fELF"):
        raise ValueError("etaHEN plugin payload is not ELF")
    return elf, meta


def unique_file_symbols(symbols: Iterable[Symbol]) -> list[str]:
    seen: set[str] = set()
    out: list[str] = []
    for symbol in symbols:
        if symbol.st_type == 4 and symbol.name and symbol.name not in seen:  # STT_FILE
            seen.add(symbol.name)
            out.append(symbol.name)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path, help="historical v1.0.0 .plugin or payload .elf")
    ap.add_argument(
        "--extract-renderer",
        type=Path,
        default=None,
        help="write embedded libphu_overlay.so to this path",
    )
    ap.add_argument("--json", type=Path, default=None, help="write machine-readable report")
    args = ap.parse_args()

    raw = args.input.read_bytes()
    raw_hash = sha256_bytes(raw)
    elf_data, plugin_meta = unwrap_plugin(raw)
    elf_hash = sha256_bytes(elf_data)
    elf = Elf64(elf_data)

    stable_plugin_match = raw_hash == KNOWN_PLUGIN_SHA256 if plugin_meta else None
    stable_elf_match = elf_hash == KNOWN_ELF_SHA256

    file_units = unique_file_symbols(elf.symbols)
    symbol_map: dict[str, list[dict[str, int]]] = {}
    for name in HIGH_VALUE_SYMBOLS:
        hits = elf.symbols_named(name)
        if hits:
            symbol_map[name] = [
                {"value": symbol.value, "size": symbol.size, "shndx": symbol.shndx}
                for symbol in hits
            ]

    renderer_info = None
    start_hits = elf.symbols_named(RENDERER_START)
    end_hits = elf.symbols_named(RENDERER_END)
    if start_hits and end_hits:
        start_va = start_hits[0].value
        end_va = end_hits[0].value
        if end_va <= start_va:
            raise ValueError("renderer end precedes start")
        start_off = elf.va_to_file_offset(start_va)
        end_off = elf.va_to_file_offset(end_va - 1) + 1
        renderer = elf_data[start_off:end_off]
        renderer_hash = sha256_bytes(renderer)
        renderer_info = {
            "start_va": start_va,
            "end_va": end_va,
            "size": len(renderer),
            "sha256": renderer_hash,
            "known_stable_match": renderer_hash == KNOWN_RENDERER_SHA256,
        }
        if args.extract_renderer:
            args.extract_renderer.parent.mkdir(parents=True, exist_ok=True)
            args.extract_renderer.write_bytes(renderer)

    report = {
        "input": str(args.input),
        "input_size": len(raw),
        "input_sha256": raw_hash,
        "plugin": plugin_meta,
        "known_plugin_match": stable_plugin_match,
        "payload_elf_size": len(elf_data),
        "payload_elf_sha256": elf_hash,
        "known_payload_match": stable_elf_match,
        "file_symbols": file_units,
        "high_value_symbols": symbol_map,
        "embedded_renderer": renderer_info,
    }

    print(json.dumps(report, ensure_ascii=False, indent=2))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    if plugin_meta and not stable_plugin_match:
        print("WARNING: plugin hash does not match historical v1.0.0")
    if not stable_elf_match:
        print("WARNING: payload ELF hash does not match historical v1.0.0")
    if renderer_info and not renderer_info["known_stable_match"]:
        print("WARNING: embedded renderer hash does not match recovered stable renderer")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
