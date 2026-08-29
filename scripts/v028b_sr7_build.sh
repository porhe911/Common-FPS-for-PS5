#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
OUT="${ROOT}/dist/v028b-sr7"

rm -rf "${ROOT}/build-v028b-sr7"
mkdir -p "${OUT}"
rm -f "${OUT}/Common_FPS_SR7_v028b_PHUF_Loopback.elf" \
      "${OUT}/Common_FPS_SR7_v028b_PHUF_Loopback_etaHEN.plugin" \
      "${OUT}/SHA256SUMS.txt" "${OUT}/LINKED_SYMBOLS.txt"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr7" -B "${ROOT}/build-v028b-sr7" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "${ROOT}/build-v028b-sr7" --target common_fps_v028b_sr7 -j"$(nproc)"

ELF="${OUT}/Common_FPS_SR7_v028b_PHUF_Loopback.elf"
PLUGIN="${OUT}/Common_FPS_SR7_v028b_PHUF_Loopback_etaHEN.plugin"
test -f "${ELF}"
test -f "${PLUGIN}"

if strings "${ELF}" | grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout|SceShellUI|elfldr'; then
  echo "ERROR: SR7 contains forbidden ptrace/MDBG/renderer symbol"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in \
  'find_game_pid_sysctl' \
  'resolve_videoout_counter' \
  'read_videoout_counter' \
  'proc_read'; do
  if ! grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt"; then
    echo "ERROR: SR7 missing shared production symbol: ${symbol}"
    exit 1
  fi
done

for marker in \
  'SR7 START production backend + PHUF loopback validation' \
  'SR7 UDP loopback ready 127.0.0.1:55541 packet_size=0x420' \
  'SR7 RX loading seq=' \
  'SR7 RX fps seq=' \
  'SR7 SUMMARY sent=' \
  'SR7 DONE clean return after 300s'; do
  if ! strings "${ELF}" | grep -Fq "${marker}"; then
    echo "ERROR: SR7 marker missing: ${marker}"
    exit 1
  fi
done

sha256sum "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
