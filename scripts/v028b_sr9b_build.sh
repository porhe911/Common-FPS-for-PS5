#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9b"

rm -rf "${ROOT}/build-v028b-sr9b"
mkdir -p "${OUT}"
rm -f "${OUT}"/*

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr9b" -B "${ROOT}/build-v028b-sr9b" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"

cmake --build "${ROOT}/build-v028b-sr9b" --target common_fps_v028b_sr9b -j"$(nproc)"

RECEIVER="${OUT}/Common_FPS_SR9B_Static_Visual_Receiver.elf"
ELF="${OUT}/Common_FPS_SR9B_v028b_Static_Visual.elf"
PLUGIN="${OUT}/Common_FPS_SR9B_v028b_Static_Visual_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"

# Receiver is allowed to create exactly the static labels, but still no hooks,
# detours, dynamic update machinery, game writes, ptrace, or MDBG transport.
if strings "${RECEIVER}" | grep -Eqi 'Detour|OnRender|pt_attach|pt_copyout|mdbg_copyout'; then
  echo "ERROR: SR9B receiver contains forbidden hook/transport code"
  exit 1
fi
for marker in \
  'id_commonfps_sr9b_label' \
  'id_commonfps_sr9b_value' \
  'UIFont' \
  'UIColor' \
  'AppendChild' \
  'FitWidthToText' \
  'FitHeightToText' \
  'FPS:' \
  'loading'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9B static visual marker missing: ${marker}"
    exit 1
  }
done

BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then
  echo "ERROR: SR9B receiver has unsupported relocations:"
  echo "${BAD_RELOCS}"
  exit 1
fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true

# Receiver bytes are intentionally embedded in the controller ELF, so visual
# strings are expected there. What must NOT happen is linking receiver visual
# routines as executable controller symbols.
if strings "${ELF}" | grep -Fq 'mdbg_copyout'; then
  echo "ERROR: SR9B controller contains forbidden MDBG transport"
  exit 1
fi
if grep -Eq '(^| )create_static_visual\(|(^| )create_label\(|(^| )append_child\(' "${OUT}/LINKED_SYMBOLS.txt"; then
  echo "ERROR: SR9B controller linked receiver visual routines as code"
  exit 1
fi

for symbol in \
  'inject_renderer_once' \
  'commonfps_v028b_elfldr_load' \
  'commonfps_v028b_import_unresolved_count' \
  'pt_attach' \
  'pt_detach' \
  'proc_read' \
  'resolve_videoout_counter'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || {
    echo "ERROR: SR9B missing required symbol: ${symbol}"
    exit 1
  }
done

for marker in \
  'SR9B START static PUI visual probe' \
  'SR9B VISUAL static only:' \
  'SR9B VISUAL READY' \
  'SR9B STATIC VISUAL PASS' \
  'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || {
    echo "ERROR: SR9B marker missing: ${marker}"
    exit 1
  }
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
