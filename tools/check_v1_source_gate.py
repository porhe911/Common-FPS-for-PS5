#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]

checks = [
    ("stable PHUF magic",
     "include/common_fps/v1_stable_wire.hpp",
     "kWireMagic = 0x46554850u"),

    ("stable UDP port 55541",
     "include/common_fps/v1_stable_wire.hpp",
     "kWirePort = 55541"),

    ("stable packet size 0x420",
     "include/common_fps/v1_stable_wire.hpp",
     "sizeof(FpsPacket) == 0x420"),

    ("DWARF last_ns field",
     "include/common_fps/v1_stable_wire.hpp",
     "last_ns"),

    ("DWARF text field",
     "include/common_fps/v1_stable_wire.hpp",
     "char text[kTextCapacity]"),

    ("stable loading payload",
     "include/common_fps/v1_stable_wire.hpp",
     '"FPS\\tloading\\n"'),

    ("stable tenth-FPS calculation",
     "include/common_fps/v1_stable_wire.hpp",
     "10000000ull"),

    ("stable VideoOut sampler",
     "src/reconstruction/v1_stable_sampler.cpp",
     '"libSceVideoOut.sprx"'),

    ("stable sysctl game discovery",
     "src/ps5/v1_stable_ps5_platform.cpp",
     "KERN_PROC_PROC"),

    ("stable eboot process name",
     "src/ps5/v1_stable_ps5_platform.cpp",
     'find_process_sysctl("eboot.bin")'),

    ("stable DMAP read model",
     "src/reconstruction/v1_stable_memory.cpp",
     "proc_read_dmap"),

    ("stable page-boundary chunking",
     "src/reconstruction/v1_stable_memory.cpp",
     "page_size - offset_in_page"),

    ("stable odd/even publisher",
     "src/reconstruction/v1_stable_hook_state.cpp",
     "sequence_.fetch_add"),

    ("stable 30-tick label delay",
     "include/common_fps/v1_stable_render_loop.hpp",
     "kLabelDelayTicks = 30"),

    ("payload args retained",
     "integration/loader/v1_loader_contract.cpp",
     "prepare_payload_args()"),

    ("Scene readiness retained",
     "integration/loader/v1_loader_contract.cpp",
     "wait_for_shellui_scene()"),

    ("image-load phase explicit",
     "integration/loader/v1_loader_contract.cpp",
     "load_renderer_image"),

    ("single loader session",
     "docs/V1_0_0_PARITY_CONTRACT.md",
     "Do not break one loader session"),

    ("DMAP parity documented",
     "docs/V1_0_0_RECONSTRUCTED_SOURCE_MAP.md",
     "prw::proc_read"),

    ("font 26",
     "include/common_fps/constants.hpp",
     "kDefaultFontSize = 26"),

    ("autoload stable readiness retained for later",
     "src/core/autoload_guard.cpp",
     "consecutive_ready_"),

    ("FW 9.60 regression documented",
     "docs/AUTOLOAD_REGRESSION_FW960.md",
     "Stop")
]

failed = False
for label, rel, needle in checks:
    text = (root / rel).read_text(encoding="utf-8")
    ok = needle in text
    print(f"{'PASS' if ok else 'FAIL'}  {label}")
    failed |= not ok

# The recovered stable shsrv order is image first, payload args second, exec
# preparation third. The old reconstruction had args before image and must not
# silently regress back to that ordering.
loader = (root / "integration/loader/v1_loader_contract.cpp").read_text(
    encoding="utf-8"
)
order = [
    loader.find("load_renderer_image"),
    loader.find("prepare_payload_args()"),
    loader.find("prepare_exec"),
]
order_ok = all(pos >= 0 for pos in order) and order == sorted(order)
print(f"{'PASS' if order_ok else 'FAIL'}  image -> payload args -> exec order")
failed |= not order_ok

# A separate post-load continue_target() belonged to the inaccurate clean-room
# model. Stable Trace Continue is internal to image loading and final resume is
# the end of the same loader session.
no_extra_continue = "continue_target()" not in loader
print(
    f"{'PASS' if no_extra_continue else 'FAIL'}  "
    "no extra post-load continue_target"
)
failed |= not no_extra_continue

# Stable v1.0.0 sampled game VideoOut through PHU's DMAP proc_read path.
# Per-read ptrace was introduced only by the later clean rewrite. Keep ptrace
# primitives completely out of the reconstructed parity core.
reconstruction_dir = root / "src" / "reconstruction"
reconstruction_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in reconstruction_dir.glob("*.cpp")
)
for forbidden in ("pt_attach(", "pt_copyout(", "pt_detach("):
    ok = forbidden not in reconstruction_text
    print(f"{'PASS' if ok else 'FAIL'}  no parity per-read ptrace: {forbidden}")
    failed |= not ok

# Hardware v2 probe proved that the clean rewrite's etaHEN allproc helper was
# not equivalent to the stable line's process discovery. Keep the reconstructed
# platform on sysctl/find_pid for game lookup/liveness.
platform = (root / "src" / "ps5" / "v1_stable_ps5_platform.cpp").read_text(
    encoding="utf-8"
)
for forbidden in ("base_.find_game_process()", "base_.process_alive("):
    ok = forbidden not in platform
    print(f"{'PASS' if ok else 'FAIL'}  no clean-rewrite process helper: {forbidden}")
    failed |= not ok

if failed:
    sys.exit(1)

print("stable v1.0.0 reconstruction source gate: PASS")
