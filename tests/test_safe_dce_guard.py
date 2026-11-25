#!/usr/bin/env python3
"""Prevent accidental PS5 game-process reads in the v1.2.6 DCE-only A/B test."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "src/ps5/commonfps_ps5_main.cpp").read_text(encoding="utf-8")
cmake = (root / "ps5/CMakeLists.txt").read_text(encoding="utf-8")
dce = (root / "src/ps5/dce_fps_sampler.cpp").read_text(encoding="utf-8")
script = (root / "scripts/ps5_source_build.sh").read_text(encoding="utf-8")

assert "COMMON_FPS_SAFE_DCE_ONLY=1" in cmake
assert '#ifndef COMMON_FPS_SAFE_DCE_ONLY' in main
assert "constexpr bool kGameProcessMemoryReadsEnabled = false;" in main

# The only calls that initiate the memory scanning sampler must be under the
# compile-time false guard; verify the matching brace, not a brittle substring.
begin = main.index("if (kGameProcessMemoryReadsEnabled) {")
open_brace = main.index("{", begin)
depth = 0
end = None
for i in range(open_brace, len(main)):
    if main[i] == "{":
        depth += 1
    elif main[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end is not None
guarded = main[begin:end]
# Match the VideoOut call, not the suffix in dce_sampler.sample().
for call in ("sampler.attach(", "process_fps = sampler.sample("):
    assert main.count(call) == 1, (call, "unexpected extra call")
    assert guarded.count(call) == 1, (call, "call escaped false guard")
assert "platform_.read_memory" not in main
assert "mdbg_copyout" not in dce
assert "read_memory(" not in dce
assert "DceFpsSampler dce_sampler(platform);" in main
assert "dce_sampler.sample()" in main
assert "SAFETY_DIAGNOSTIC=1 game_memory_reads=0 game_memory_writes=0" in main
assert "test_safe_dce_guard.py" in script
assert "Common_FPS_PS5_v1.2.6.elf" in cmake
assert "--version 1.26" in cmake
print("v1.2.6 DCE-only compile gate/source integration: PASS")
