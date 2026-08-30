#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9e"

rm -rf "${ROOT}/build-v028b-sr9e"
mkdir -p "${OUT}"
rm -f "${OUT}"/*

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr9e" -B "${ROOT}/build-v028b-sr9e" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"

cmake --build "${ROOT}/build-v028b-sr9e" --target common_fps_v028b_sr9e -j"$(nproc)"

RECEIVER="${OUT}/Common_FPS_SR9E_Delegate_Construct_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9E_v028b_Delegate_Construct.elf"
PLUGIN="${OUT}/Common_FPS_SR9E_v028b_Delegate_Construct_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"

# SR9E must remain construction-only inside ShellUI.
if strings "${RECEIVER}" | grep -Eqi 'EnqueueEventAction|CreateLabel|AppendChild|Set_Property|TextColor|FitWidthToText|DetourFunction|WriteJump|PatchInJump|OnRender_Hook|pt_attach|pt_copyout|mdbg_copyout'; then
  echo "ERROR: SR9E receiver contains forbidden enqueue/UI mutation/hook code"
  exit 1
fi

for marker in \
  'START_DELEGATE_CONSTRUCT_ONLY' \
  'mono_object_new' \
  'mono_compile_method' \
  'mono_runtime_invoke' \
  'RequestSwapBuffersIfNoDraw' \
  'System' \
  'Action' \
  'DELEGATE_CONSTRUCTED' \
  'DONE_NO_INVOKE_NO_ENQUEUE'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9E construction marker missing: ${marker}"
    exit 1
  }
done

BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then
  echo "ERROR: SR9E receiver has unsupported relocations:"
  echo "${BAD_RELOCS}"
  exit 1
fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in \
  'inject_renderer_once' \
  'commonfps_v028b_elfldr_load' \
  'commonfps_v028b_import_unresolved_count' \
  'pt_attach' \
  'pt_detach' \
  'find_process_pid_sysctl'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || {
    echo "ERROR: SR9E missing required controller symbol: ${symbol}"
    exit 1
  }
done

for marker in \
  'SR9E START System.Action construction-only probe' \
  'NO EnqueueEventAction / NO delegate invoke / NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch' \
  'SR9E RESULT code=' \
  'SR9E PASS System.Action constructed and verified' \
  'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9E controller marker missing: ${marker}"
    exit 1
  }
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
