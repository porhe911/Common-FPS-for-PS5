#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
SHSRV_SRC="${ROOT}/.deps/shsrv"
OUT="${ROOT}/dist/v028b-sr4"

rm -rf "${ROOT}/build-v028b-sr4"
mkdir -p "${OUT}"
rm -f "${OUT}/Common_FPS_SR4_v028b_V5_RC12_DMAP.elf" \
      "${OUT}/Common_FPS_SR4_v028b_V5_RC12_DMAP_etaHEN.plugin" \
      "${OUT}/SHA256SUMS.txt" "${OUT}/LINKED_SYMBOLS.txt"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" -B "${ROOT}/build-v028b-sr4" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${SHSRV_SRC}"

cmake --build "${ROOT}/build-v028b-sr4" --target common_fps_v028b_sr4 -j"$(nproc)"

ELF="${OUT}/Common_FPS_SR4_v028b_V5_RC12_DMAP.elf"
PLUGIN="${OUT}/Common_FPS_SR4_v028b_V5_RC12_DMAP_etaHEN.plugin"
test -f "${ELF}"
test -f "${PLUGIN}"

# SR4 must remain free of the failed RC transports and visual injection code.
if strings "${ELF}" | grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout|SceShellUI|elfldr'; then
  echo "ERROR: SR4 contains forbidden ptrace/MDBG/renderer symbol"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for sym in find_game_pid_sysctl translate proc_read; do
  if ! grep -q "${sym}" "${OUT}/LINKED_SYMBOLS.txt"; then
    echo "ERROR: SR4 missing recovered backend symbol: ${sym}"
    exit 1
  fi
done

for marker in \
  'SR4 MODULE size-query rc=12 accepted as FW960 expected result' \
  'SR4 MODULE FOUND' \
  'SR4 AUTH restored before DMAP reads' \
  'SR4 DMAP table read OK' \
  'SR4 COUNTER READY'; do
  if ! strings "${ELF}" | grep -q "${marker}"; then
    echo "ERROR: SR4 diagnostic marker missing: ${marker}"
    exit 1
  fi
done

sha256sum "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
