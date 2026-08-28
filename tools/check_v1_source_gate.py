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

    ("production worker uses AutoloadGuard",
     "src/ps5/commonfps_ps5_main.cpp",
     "AutoloadGuard autoload_guard"),

    ("game lifecycle gated by visual readiness",
     "src/ps5/commonfps_ps5_main.cpp",
     "autoload_backend.visual_ready()"),

    ("real PS5 autoload backend",
     "src/ps5/ps5_autoload_backend.cpp",
     'find_proc_by_name("SceShellUI")'),

    ("full source loader payload args retained",
     "src/ps5/ps5_autoload_backend.cpp",
     "elfldr_debug(-1, -1, -1"),

    ("ShellUI never killed on loader failure",
     "src/ps5/ps5_autoload_backend.cpp",
     "pt_detach(target_pid, 0)"),

    ("single-file embedded renderer",
     "src/ps5/ps5_autoload_backend.cpp",
     "embedded::kShellUIElf"),

    ("build-time renderer embedding",
     "ps5/CMakeLists.txt",
     "tools/embed_binary.py"),

    ("renderer receiver heartbeat",
     "src/ps5/shellui_payload/commonfps_shellui_entry.cpp",
     "RendererHealthPhase::ReceiverReady"),

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

backend_text = (root / "src/ps5/ps5_autoload_backend.cpp").read_text(
    encoding="utf-8"
)
for forbidden in (
    "/data/CommonFPS/Common_FPS_ShellUI_v1.1.0.elf",
    "/data/etaHEN/plugins/Common_FPS_ShellUI_v1.1.0.elf",
):
    ok = forbidden not in backend_text
    print(
        f"{'PASS' if ok else 'FAIL'}  "
        f"no external renderer dependency: {forbidden}"
    )
    failed |= not ok

if failed:
    sys.exit(1)

print("v1.0.0 + safe-autoload source gate: PASS")
