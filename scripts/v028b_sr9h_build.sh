#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9h"
rm -rf "${ROOT}/build-v028b-sr9h"
mkdir -p "${OUT}"
rm -f "${OUT}"/*
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S "${ROOT}/ps5/sr9h" -B "${ROOT}/build-v028b-sr9h" -DCMAKE_BUILD_TYPE=Release -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"
cmake --build "${ROOT}/build-v028b-sr9h" --target common_fps_v028b_sr9h -j"$(nproc)"
RECEIVER="${OUT}/Common_FPS_SR9H_Widget_Signature_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9H_v028b_Widget_Signatures.elf"
PLUGIN="${OUT}/Common_FPS_SR9H_v028b_Widget_Signatures_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"
if strings "${RECEIVER}" | grep -Eqi 'CreateLabel|AppendChild|RemoveFromParent\(|Set_Property|TextColor|DetourFunction|WriteJump|PatchInJump|OnRender_Hook|pt_attach|pt_copyout|mdbg_copyout'; then
  echo "ERROR: SR9H receiver contains forbidden UI invocation/mutation/hook code"; exit 1
fi
for marker in 'START_WIDGET_SIGNATURE_ONLY' 'Widget' 'mono_class_get_methods' 'mono_method_signature' 'DONE'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || { echo "ERROR: SR9H metadata marker missing: ${marker}"; exit 1; }
done
BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then echo "ERROR: SR9H receiver has unsupported relocations:"; echo "${BAD_RELOCS}"; exit 1; fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true
nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in 'inject_renderer_once' 'commonfps_v028b_elfldr_load' 'commonfps_v028b_import_unresolved_count' 'pt_attach' 'pt_detach' 'find_process_pid_sysctl'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || { echo "ERROR: SR9H missing controller symbol: ${symbol}"; exit 1; }
done
for marker in 'SR9H START Widget zero-arg signature metadata probe' 'NO Widget invocation / NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch' 'SR9H SIGNATURE name=' 'SR9H PASS Widget signature capture complete'; do
  strings "${ELF}" | grep -Fq "${marker}" || { echo "ERROR: SR9H controller marker missing: ${marker}"; exit 1; }
done
sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
