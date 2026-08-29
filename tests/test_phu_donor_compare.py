#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from compare_phu_r11_donor import (
    KNOWN_DONOR_SHA256,
    KNOWN_STABLE_ELF_SHA256,
    diff_runs,
)


def main() -> int:
    assert diff_runs(b"abc", b"abc") == []
    assert diff_runs(b"abc", b"axc") == [(1, 2)]
    assert diff_runs(b"abcdef", b"abXXef") == [(2, 4)]
    assert diff_runs(b"ab", b"abcd") == [(2, 4)]
    assert len(KNOWN_DONOR_SHA256) == 64
    assert len(KNOWN_STABLE_ELF_SHA256) == 64
    print("PHU donor compare helper tests: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
