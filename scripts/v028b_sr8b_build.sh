#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr8b"

rm -rf "${ROOT}/build-v028b-sr8b"
mkdir -p "${OUT}"
rm -f "${OUT}"/*

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr8b" -B "${ROOT}/build-v028b-sr8b" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"

cmake --build "${ROOT}/build-v028b-sr8b" --target common_fps_v028b_sr8b -j"$(nproc)"

RECEIVER="${OUT}/Common_FPS_SR8B_ReceiverUDP.elf"
ELF="${OUT}/Common_FPS_SR8B_v028b_Receiver_UDP.elf"
PLUGIN="${OUT}/Common_FPS_SR8B_v028b_Receiver_UDP_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"

# Receiver stays UI/file-system free: only PHUF 55541 + health 55542.
if strings "${RECEIVER}" | grep -Eqi 'Mono|PUI|CreateLabel|OnRender|SceShellUI|Detour|CommonFPS_SR8A_receiver_health|/data/'; then
  echo "ERROR: SR8B receiver unexpectedly contains UI or filesystem code"
  exit 1
fi
for marker in 'RH8B' '55541' '55542'; do
  strings "${RECEIVER}" | grep -Fq "${marker}" || true
done

BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then
  echo "ERROR: SR8B receiver has unsupported relocations:"
  echo "${BAD_RELOCS}"
  exit 1
fi
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true

# Controller may contain ptrace only for the one-time bootstrap.
if strings "${ELF}" | grep -Eqi 'mdbg_copyout|CreateLabel|OnRender'; then
  echo "ERROR: SR8B controller contains forbidden MDBG/visual renderer code"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in \
  'inject_renderer_once' \
  'commonfps_v028b_elfldr_load' \
  'commonfps_v028b_elfldr_payload_args' \
  'commonfps_v028b_import_unresolved_count' \
  'pt_attach' \
  'pt_detach' \
  'proc_read' \
  'resolve_videoout_counter'; do
  grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt" || {
    echo "ERROR: SR8B missing required symbol: ${symbol}"
    exit 1
  }
done

for marker in \
  'SR8B START UDP-health receiver-only ShellUI bootstrap' \
  'SR8B NO files / NO PUI / NO Mono / NO UI hook / one injection only' \
  'SR8B HEALTH receiver READY via UDP' \
  'SR8B RECEIVER READY PASS; begin production PHUF lifecycle' \
  'imports_unresolved='; do
  strings "${ELF}" | grep -Fq "${marker}" || {
    echo "ERROR: SR8B marker missing: ${marker}"
    exit 1
  }
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
