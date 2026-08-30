#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9c"

rm -rf "${ROOT}/build-v028b-sr9c"
mkdir -p "${OUT}"
rm -f "${OUT}"/*

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr9c" -B "${ROOT}/build-v028b-sr9c" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"

cmake --build "${ROOT}/build-v028b-sr9c" --target common_fps_v028b_sr9c -j"$(nproc)"

RECEIVER="${OUT}/Common_FPS_SR9C_Dispatcher_Discovery_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9C_v028b_Dispatcher_Discovery.elf"
PLUGIN="${OUT}/Common_FPS_SR9C_v028b_Dispatcher_Discovery_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"

# SR9C is deliberately metadata-only inside ShellUI: absolutely no UI object
# creation/mutation and no hook/detour/code-patch implementation.
if strings "${RECEIVER}" | grep -Eqi 'CreateLabel|AppendChild|Set_Property|TextColor|FitWidthToText|DetourFunction|WriteJump|PatchInJump|OnRender_Hook|pt_attach|pt_copyout|mdbg_copyout'; then
  echo "ERROR: SR9C receiver contains forbidden UI mutation/hook/transport code"
  exit 1
fi

for marker in \
  'START_METADATA_ONLY' \
  'mono_class_get_methods' \
  'mono_method_get_name' \
  'mono_method_signature' \
  'mono_signature_get_param_count' \
  'Sce.PlayStation.PUI' \
  'Application' \
  'Dispatcher' \
  'UIThread' \
  'DONE'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9C metadata marker missing: ${marker}"
    exit 1
  }
done

BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then
  echo "ERROR: SR9C receiver has unsupported relocations:"
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
    echo "ERROR: SR9C missing required controller symbol: ${symbol}"
    exit 1
  }
done

for marker in \
  'SR9C START metadata-only PUI dispatcher discovery' \
  'NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch' \
  'SR9C METHOD class_id=' \
  'SR9C PASS metadata discovery complete' \
  'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9C controller marker missing: ${marker}"
    exit 1
  }
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
