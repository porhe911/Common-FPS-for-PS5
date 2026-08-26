#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]

checks = [
    ("integer FPS",
     "include/common_fps/types.hpp",
     "int fps = 0"),

    ("font 26",
     "include/common_fps/constants.hpp",
     "kDefaultFontSize = 26"),

    ("payload args",
     "integration/loader/v1_loader_contract.cpp",
     "prepare_payload_args()"),

    ("Scene readiness",
     "integration/loader/v1_loader_contract.cpp",
     "wait_for_shellui_scene()"),

    ("single loader session",
     "docs/V1_0_0_PARITY_CONTRACT.md",
     "Do not break one loader session"),

    ("autoload stable readiness",
     "src/core/autoload_guard.cpp",
     "consecutive_ready_"),

    ("autoload retry",
     "src/core/autoload_guard.cpp",
     "schedule_retry"),

    ("autoload renderer health",
     "src/core/autoload_guard.cpp",
     "renderer_alive()"),

    ("scene self-heal",
     "src/core/scene_guard.cpp",
     "widgets_alive()"),

    ("FW 9.60 autoload regression documented",
     "docs/AUTOLOAD_REGRESSION_FW960.md",
     "Stop")
]

failed = False

for label, rel, needle in checks:
    text = (root / rel).read_text(encoding="utf-8")
    ok = needle in text
    print(f"{'PASS' if ok else 'FAIL'}  {label}")
    failed |= not ok

if failed:
    sys.exit(1)

print("v1.0.0 + safe-autoload source gate: PASS")
