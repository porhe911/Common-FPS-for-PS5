#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9a"

rm -rf "${ROOT}/build-v028b-sr9a"
mkdir -p "${OUT}"
rm -f "${OUT}"/*

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr9a" -B "${ROOT}/build-v028b-sr9a" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"

cmake --build "${ROOT}/build-v028b-sr9a" --target common_fps_v028b_sr9a -j"$(nproc)"

RECEIVER="${OUT}/Common_FPS_SR9A_PUI_Context_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9A_v028b_PUI_Context.elf"
PLUGIN="${OUT}/Common_FPS_SR9A_v028b_PUI_Context_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"

# SR9A may resolve Mono/PUI metadata but must not create or mutate UI.
if strings "${RECEIVER}" | grep -Eqi 'CreateLabel|AppendChild|RemoveFromParent|OnRender|Detour|CreateUIFont|CreateUIColor'; then
  echo "ERROR: SR9A receiver contains visual/UI mutation code"
  exit 1
fi
for marker in \
  'Sce.PlayStation.PUI.dll' \
  'Sce.Vsh.ShellUI.AppSystem.dll' \
  'FindContainerSceneByPath' \
  'RootWidget' \
  'libmonosgen-2.0.sprx'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9A context marker missing: ${marker}"
    exit 1
  }
done

BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then
  echo "ERROR: SR9A receiver has unsupported relocations:"
  echo "${BAD_RELOCS}"
  exit 1
fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true

# Controller has ptrace only for the already-proven one-time ShellUI bootstrap.
if strings "${ELF}" | grep -Eqi 'mdbg_copyout|CreateLabel|AppendChild|OnRender'; then
  echo "ERROR: SR9A controller contains forbidden MDBG/visual code"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in \
  'inject_renderer_once' \
  'commonfps_v028b_elfldr_load' \
  'commonfps_v028b_import_unresolved_count' \
  'pt_attach' \
  'pt_detach' \
  'proc_read' \
  'resolve_videoout_counter'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || {
    echo "ERROR: SR9A missing required symbol: ${symbol}"
    exit 1
  }
done

for marker in \
  'SR9A START PUI context-only ShellUI probe' \
  'SR9A NO CreateLabel / NO AppendChild / NO Detour / NO UI hook' \
  'SR9A CONTEXT READY' \
  'SR9A PUI CONTEXT PASS; begin unchanged production PHUF lifecycle' \
  'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9A marker missing: ${marker}"
    exit 1
  }
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
