#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
SHSRV_SRC="${ROOT}/.deps/shsrv"
OUT="${ROOT}/dist/v028b-sr5"

rm -rf "${ROOT}/build-v028b-sr5"
mkdir -p "${OUT}"
rm -f "${OUT}/Common_FPS_SR5_v028b_DMAP_Stage_Trace.elf" \
      "${OUT}/Common_FPS_SR5_v028b_DMAP_Stage_Trace_etaHEN.plugin" \
      "${OUT}/SHA256SUMS.txt" "${OUT}/LINKED_SYMBOLS.txt"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" -B "${ROOT}/build-v028b-sr5" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${SHSRV_SRC}"

cmake --build "${ROOT}/build-v028b-sr5" --target common_fps_v028b_sr5 -j"$(nproc)"

ELF="${OUT}/Common_FPS_SR5_v028b_DMAP_Stage_Trace.elf"
PLUGIN="${OUT}/Common_FPS_SR5_v028b_DMAP_Stage_Trace_etaHEN.plugin"
test -f "${ELF}"
test -f "${PLUGIN}"

if strings "${ELF}" | grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout|SceShellUI|elfldr'; then
  echo "ERROR: SR5 contains forbidden ptrace/MDBG/renderer symbol"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
if ! grep -q 'find_game_pid_sysctl' "${OUT}/LINKED_SYMBOLS.txt"; then
  echo "ERROR: SR5 missing stable sysctl lifecycle symbol"
  exit 1
fi

# Some stage labels are passed as arguments to the generic TRACE logger, so
# require the literal label and the generic TRACE format independently.
for marker in \
  'SR5 START exact-reference DMAP stage trace' \
  'SR5 AUTH restored before all DMAP work' \
  'SR5 TRACE sdk sysctl' \
  'SR5 TRACE %s rc=' \
  'allproc_head' \
  'pmap' \
  'PML4E' \
  'final_DMAP_copyout' \
  'SR5 COUNTER READY'; do
  if ! strings "${ELF}" | grep -q "${marker}"; then
    echo "ERROR: SR5 diagnostic marker missing: ${marker}"
    exit 1
  fi
done

sha256sum "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
