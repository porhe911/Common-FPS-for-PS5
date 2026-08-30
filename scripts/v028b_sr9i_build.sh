#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9i"
rm -rf "${ROOT}/build-v028b-sr9i"; mkdir -p "${OUT}"; rm -f "${OUT}"/*
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S "${ROOT}/ps5/sr9i" -B "${ROOT}/build-v028b-sr9i" -DCMAKE_BUILD_TYPE=Release -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"
cmake --build "${ROOT}/build-v028b-sr9i" --target common_fps_v028b_sr9i -j"$(nproc)"
RECEIVER="${OUT}/Common_FPS_SR9I_Layout_Queue_Receiver.elf"; ELF="${OUT}/Common_FPS_SR9I_v028b_Layout_Queue.elf"; PLUGIN="${OUT}/Common_FPS_SR9I_v028b_Layout_Queue_etaHEN.plugin"
test -f "$RECEIVER" && test -f "$ELF" && test -f "$PLUGIN"
if strings "$RECEIVER" | grep -Eqi 'CreateLabel|AppendChild|RemoveFromParent|FindWidgetByName|DetourFunction|WriteJump|PatchInJump|OnRender_Hook|pt_attach|pt_copyout|mdbg_copyout'; then echo "ERROR: SR9I receiver contains forbidden visual/tree mutation/hook code"; exit 1; fi
for marker in 'START_LAYOUT_QUEUE_PROOF' 'LayoutIfNeeded' 'EnqueueEventAction' 'LAYOUT_THEN_CLEAR_READY' 'COUNT_BEFORE' 'COUNT_AFTER' 'PASS_LAYOUT_IF_NEEDED_THROUGH_UI_QUEUE'; do strings "$RECEIVER" | grep -Fq "$marker" || { echo "ERROR: SR9I marker missing: $marker"; exit 1; }; done
BAD_RELOCS="$(readelf -r "$RECEIVER" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"; if [[ -n "$BAD_RELOCS" ]]; then echo "ERROR: SR9I unsupported relocations:"; echo "$BAD_RELOCS"; exit 1; fi
readelf -r "$RECEIVER" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true
nm -C "$ELF" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in 'inject_renderer_once' 'commonfps_v028b_elfldr_load' 'commonfps_v028b_import_unresolved_count' 'pt_attach' 'pt_detach' 'find_process_pid_sysctl'; do grep -q "$symbol" "${OUT}/LINKED_SYMBOLS.txt" || { echo "ERROR: SR9I missing controller symbol: $symbol"; exit 1; }; done
sha256sum "$RECEIVER" "$ELF" "$PLUGIN" > "${OUT}/SHA256SUMS.txt"; cat "${OUT}/SHA256SUMS.txt"
