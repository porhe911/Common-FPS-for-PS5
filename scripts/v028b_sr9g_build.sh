#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9g"
rm -rf "${ROOT}/build-v028b-sr9g"
mkdir -p "${OUT}"
rm -f "${OUT}"/*
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S "${ROOT}/ps5/sr9g" -B "${ROOT}/build-v028b-sr9g" -DCMAKE_BUILD_TYPE=Release -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"
cmake --build "${ROOT}/build-v028b-sr9g" --target common_fps_v028b_sr9g -j"$(nproc)"
RECEIVER="${OUT}/Common_FPS_SR9G_Native_Action_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9G_v028b_Native_Action.elf"
PLUGIN="${OUT}/Common_FPS_SR9G_v028b_Native_Action_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"
if strings "${RECEIVER}" | grep -Eqi 'CreateLabel|AppendChild|Set_Property|TextColor|FitWidthToText|RemoveFromParent|DetourFunction|WriteJump|PatchInJump|OnRender_Hook|pt_attach|pt_copyout|mdbg_copyout'; then
  echo "ERROR: SR9G receiver contains forbidden UI mutation/hook code"; exit 1
fi
for marker in 'START_NATIVE_ACTION_PROOF' 'GetDelegateForFunctionPointer' 'NATIVE_DELEGATE_READY' 'NATIVE_ACTION_SUBMITTED' 'NATIVE_CALLBACK_SEEN' 'PASS_NATIVE_ACTION_EXECUTED_NO_UI_MUTATION'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || { echo "ERROR: SR9G marker missing: ${marker}"; exit 1; }
done
BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then echo "ERROR: SR9G receiver has unsupported relocations:"; echo "${BAD_RELOCS}"; exit 1; fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true
nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in 'inject_renderer_once' 'commonfps_v028b_elfldr_load' 'commonfps_v028b_import_unresolved_count' 'pt_attach' 'pt_detach' 'find_process_pid_sysctl'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || { echo "ERROR: SR9G missing controller symbol: ${symbol}"; exit 1; }
done
for marker in 'SR9G START native System.Action through PUI queue proof' 'NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch' 'SR9G RESULT value=' 'SR9G PASS native callback executed through PUI queue' 'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || { echo "ERROR: SR9G controller marker missing: ${marker}"; exit 1; }
done
sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
