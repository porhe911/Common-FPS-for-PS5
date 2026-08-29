#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_v1_stable import Elf64, unwrap_plugin, sha256_bytes

KNOWN_DONOR_SHA256 = "8e20deefb9100705be8352dc6acb47241c6a044b93dc3f578f93c424789b2622"
KNOWN_STABLE_ELF_SHA256 = "6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c"
KNOWN_DONOR_RENDERER_SHA256 = "8d1490ac7d1ba589044108675e29a1d3cc05a0a946556d52aeb502d114ea7eca"
KNOWN_STABLE_RENDERER_SHA256 = "3640cbcaf907bc900e257503b3c8922fdbc129c492a76a2348fd75c84f68b91c"
RENDERER_START = "_binary_libphu_overlay_so_start"
RENDERER_END = "_binary_libphu_overlay_so_end"


def section_bytes(elf: Elf64, name: str) -> bytes:
    sec = next((s for s in elf.sections if s.name == name), None)
    if sec is None or sec.sh_type == 8:
        return b""
    return elf.data[sec.offset: sec.offset + sec.size]


def build_id(elf: Elf64) -> str | None:
    blob = section_bytes(elf, ".note.gnu.build-id")
    if len(blob) < 16:
        return None
    namesz = int.from_bytes(blob[0:4], "little")
    descsz = int.from_bytes(blob[4:8], "little")
    desc_off = (12 + namesz + 3) & ~3
    if desc_off + descsz > len(blob):
        return None
    return blob[desc_off:desc_off + descsz].hex()


def extract_renderer(elf: Elf64) -> bytes:
    starts = elf.symbols_named(RENDERER_START)
    ends = elf.symbols_named(RENDERER_END)
    if not starts or not ends:
        raise ValueError("embedded renderer symbols missing")
    start = starts[0].value
    end = ends[0].value
    if end <= start:
        raise ValueError("invalid renderer range")
    start_off = elf.va_to_file_offset(start)
    end_off = elf.va_to_file_offset(end - 1) + 1
    return elf.data[start_off:end_off]


def diff_runs(a: bytes, b: bytes) -> list[tuple[int, int]]:
    n = min(len(a), len(b))
    positions = [i for i in range(n) if a[i] != b[i]]
    if len(a) != len(b):
        positions.extend(range(n, max(len(a), len(b))))
    if not positions:
        return []

    runs: list[tuple[int, int]] = []
    start = previous = positions[0]
    for pos in positions[1:]:
        if pos == previous + 1:
            previous = pos
            continue
        runs.append((start, previous + 1))
        start = previous = pos
    runs.append((start, previous + 1))
    return runs


def touched_functions(elf: Elf64, section_name: str, runs: list[tuple[int, int]]) -> list[str]:
    sec = next((s for s in elf.sections if s.name == section_name), None)
    if sec is None:
        return []

    funcs = sorted(
        (s.value, s.name)
        for s in elf.symbols
        if s.st_type == 2
        and s.name
        and s.shndx == sec.index
        and sec.addr <= s.value < sec.addr + sec.size
    )

    out: list[str] = []
    for start, _end in runs:
        va = sec.addr + start
        name = None
        for func_va, func_name in funcs:
            if func_va > va:
                break
            name = func_name
        if name and name not in out:
            out.append(name)
    return out


def compare_alloc_sections(a: Elf64, b: Elf64) -> dict[str, dict[str, int]]:
    out: dict[str, dict[str, int]] = {}
    for sa in a.sections:
        if not (sa.flags & 0x2) or sa.sh_type == 8:
            continue
        sb = next((s for s in b.sections if s.name == sa.name), None)
        if sb is None or sb.sh_type == 8:
            continue
        da = a.data[sa.offset:sa.offset + sa.size]
        db = b.data[sb.offset:sb.offset + sb.size]
        changed = sum(x != y for x, y in zip(da, db)) + abs(len(da) - len(db))
        if changed:
            out[sa.name] = {
                "donor_size": len(da),
                "stable_size": len(db),
                "changed_bytes": changed,
            }
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Compare PHU v1.14.25-r11 donor to Common FPS v1.0.0"
    )
    ap.add_argument("donor_elf", type=Path)
    ap.add_argument("stable", type=Path, help="Common FPS v1.0.0 .plugin or .elf")
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()

    donor_raw = args.donor_elf.read_bytes()
    stable_raw = args.stable.read_bytes()
    stable_elf_raw, _meta = unwrap_plugin(stable_raw)

    donor = Elf64(donor_raw)
    stable = Elf64(stable_elf_raw)
    donor_renderer_raw = extract_renderer(donor)
    stable_renderer_raw = extract_renderer(stable)
    donor_renderer = Elf64(donor_renderer_raw)
    stable_renderer = Elf64(stable_renderer_raw)

    outer_text_a = section_bytes(donor, ".text")
    outer_text_b = section_bytes(stable, ".text")
    outer_runs = diff_runs(outer_text_a, outer_text_b)

    renderer_text_a = section_bytes(donor_renderer, ".text")
    renderer_text_b = section_bytes(stable_renderer, ".text")
    renderer_runs = diff_runs(renderer_text_a, renderer_text_b)
    renderer_ro_a = section_bytes(donor_renderer, ".rodata")
    renderer_ro_b = section_bytes(stable_renderer, ".rodata")

    report = {
        "donor": {
            "size": len(donor_raw),
            "sha256": sha256_bytes(donor_raw),
            "known_match": sha256_bytes(donor_raw) == KNOWN_DONOR_SHA256,
            "build_id": build_id(donor),
        },
        "stable": {
            "size": len(stable_elf_raw),
            "sha256": sha256_bytes(stable_elf_raw),
            "known_match": sha256_bytes(stable_elf_raw) == KNOWN_STABLE_ELF_SHA256,
            "build_id": build_id(stable),
        },
        "outer_text": {
            "changed_bytes": sum(end - start for start, end in outer_runs),
            "changed_runs": len(outer_runs),
            "touched_functions": touched_functions(donor, ".text", outer_runs),
        },
        "renderer": {
            "size": len(donor_renderer_raw),
            "donor_sha256": sha256_bytes(donor_renderer_raw),
            "stable_sha256": sha256_bytes(stable_renderer_raw),
            "donor_known_match": sha256_bytes(donor_renderer_raw) == KNOWN_DONOR_RENDERER_SHA256,
            "stable_known_match": sha256_bytes(stable_renderer_raw) == KNOWN_STABLE_RENDERER_SHA256,
            "donor_build_id": build_id(donor_renderer),
            "stable_build_id": build_id(stable_renderer),
            "text_changed_bytes": sum(end - start for start, end in renderer_runs),
            "rodata_changed_bytes": (
                sum(x != y for x, y in zip(renderer_ro_a, renderer_ro_b))
                + abs(len(renderer_ro_a) - len(renderer_ro_b))
            ),
            "touched_functions": touched_functions(donor_renderer, ".text", renderer_runs),
            "alloc_section_diff": compare_alloc_sections(donor_renderer, stable_renderer),
        },
    }

    print(json.dumps(report, indent=2))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
