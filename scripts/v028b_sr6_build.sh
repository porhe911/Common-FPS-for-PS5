#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
SHSRV_SRC="${ROOT}/.deps/shsrv"
OUT="${ROOT}/dist/v028b-sr6"

rm -rf "${ROOT}/build-v028b-sr6"
mkdir -p "${OUT}"
rm -f "${OUT}/Common_FPS_SR6_v028b_Production_Backend.elf" \
      "${OUT}/Common_FPS_SR6_v028b_Production_Backend_etaHEN.plugin" \
      "${OUT}/SHA256SUMS.txt" "${OUT}/LINKED_SYMBOLS.txt"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" -B "${ROOT}/build-v028b-sr6" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${SHSRV_SRC}"

cmake --build "${ROOT}/build-v028b-sr6" --target common_fps_v028b_sr6 -j"$(nproc)"

ELF="${OUT}/Common_FPS_SR6_v028b_Production_Backend.elf"
PLUGIN="${OUT}/Common_FPS_SR6_v028b_Production_Backend_etaHEN.plugin"
test -f "${ELF}"
test -f "${PLUGIN}"

if strings "${ELF}" | grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout|SceShellUI|elfldr'; then
  echo "ERROR: SR6 contains forbidden ptrace/MDBG/renderer symbol"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in \
  'find_game_pid_sysctl' \
  'resolve_videoout_counter' \
  'read_videoout_counter' \
  'proc_read'; do
  if ! grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt"; then
    echo "ERROR: SR6 missing shared production symbol: ${symbol}"
    exit 1
  fi
done

# The SR6 wrapper must not carry a private copy of the SR5 trace translator.
if strings "${ELF}" | grep -q 'SR5 TRACE'; then
  echo "ERROR: SR6 unexpectedly contains SR5 private trace implementation"
  exit 1
fi

for marker in \
  'SR6 START production backend validation' \
  'SR6 USES shared proc_rw_v960 + shared videoout_counter' \
  'SR6 WARMUP first delta discarded' \
  'SR6 FPS sanity reject >130.0; keep counter attached' \
  'SR6 COUNTER READY'; do
  if ! strings "${ELF}" | grep -Fq "${marker}"; then
    echo "ERROR: SR6 marker missing: ${marker}"
    exit 1
  fi
done

sha256sum "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
