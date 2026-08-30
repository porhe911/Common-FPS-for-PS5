#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9l"
rm -rf "${ROOT}/build-v028b-sr9l"
mkdir -p "${OUT}"
rm -f "${OUT}"/*
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S "${ROOT}/ps5/sr9l" -B "${ROOT}/build-v028b-sr9l" -DCMAKE_BUILD_TYPE=Release -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"
cmake --build "${ROOT}/build-v028b-sr9l" --target common_fps_v028b_sr9l -j"$(nproc)"
RECEIVER="${OUT}/Common_FPS_SR9L_Label_Text_Queue_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9L_v028b_Label_Text_Queue.elf"
PLUGIN="${OUT}/Common_FPS_SR9L_v028b_Label_Text_Queue_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"
if strings "${RECEIVER}" | grep -Eqi 'AppendChild|RemoveFromParent|CreateUIFont|UIColor|TextColor|FitWidthToText|FitHeightToText|HorizontalAlignment|VerticalAlignment|set_X|set_Y|set_Name|DetourFunction|WriteJump|PatchInJump|OnRender_Hook|pt_attach|pt_copyout|mdbg_copyout'; then
  echo "ERROR: SR9L receiver contains forbidden tree/style/hook code"; exit 1
fi
for marker in 'START_LABEL_TEXT_QUEUE_PROOF' 'FPS TEST' 'OBJECT_AND_VALUE_READY_OFF_TREE' 'CTOR_TEXT_CLEAR_CHAIN_READY' 'TEXT_SETTER_STATUS' 'TEXT_VERIFY' 'PASS_LABEL_TEXT_THROUGH_NATIVE_UI_CALLBACK'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || { echo "ERROR: SR9L marker missing: ${marker}"; exit 1; }
done
BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then echo "ERROR: SR9L receiver has unsupported relocations:"; echo "${BAD_RELOCS}"; exit 1; fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true
nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in 'inject_renderer_once' 'commonfps_v028b_elfldr_load' 'commonfps_v028b_import_unresolved_count' 'pt_attach' 'pt_detach' 'find_process_pid_sysctl'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || { echo "ERROR: SR9L missing controller symbol: ${symbol}"; exit 1; }
done
for marker in 'SR9L START one queued off-tree Label.Text setter proof' 'NO AppendChild / NO RemoveFromParent / NO X/Y/Name/font/color / NO dynamic FPS / NO Detour / NO code patch' 'SR9L RESULT value=' 'SR9L PASS Label.Text set and verified through native PUI callback' 'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || { echo "ERROR: SR9L controller marker missing: ${marker}"; exit 1; }
done
sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
