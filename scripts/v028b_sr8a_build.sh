#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr8a"

rm -rf "${ROOT}/build-v028b-sr8a"
mkdir -p "${OUT}"
rm -f "${OUT}"/*

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5/sr8a" -B "${ROOT}/build-v028b-sr8a" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"

cmake --build "${ROOT}/build-v028b-sr8a" --target common_fps_v028b_sr8a -j"$(nproc)"

RECEIVER="${OUT}/Common_FPS_SR8A_ReceiverOnly.elf"
ELF="${OUT}/Common_FPS_SR8A_v028b_Receiver_Bootstrap.elf"
PLUGIN="${OUT}/Common_FPS_SR8A_v028b_Receiver_Bootstrap_etaHEN.plugin"
test -f "${RECEIVER}" && test -f "${ELF}" && test -f "${PLUGIN}"

# Receiver must stay completely UI-free.
if strings "${RECEIVER}" | grep -Eqi 'Mono|PUI|CreateLabel|OnRender|SceShellUI|Detour'; then
  echo "ERROR: SR8A receiver unexpectedly contains UI/Mono/hook code"
  exit 1
fi

# Controller intentionally contains ptrace only for ONE ShellUI bootstrap.
# MDBG and the old Stage-B renderer must never enter this target.
if strings "${ELF}" | grep -Eqi 'mdbg_copyout|CommonFPSRenderer|CreateLabel|OnRender'; then
  echo "ERROR: SR8A controller contains forbidden MDBG/visual renderer code"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for symbol in \
  'inject_renderer_once' \
  'commonfps_v028b_elfldr_load' \
  'commonfps_v028b_elfldr_payload_args' \
  'commonfps_v028b_trace_continue_seen' \
  'commonfps_v028b_trace_stop_seen' \
  'pt_attach' \
  'pt_detach' \
  'proc_read' \
  'resolve_videoout_counter'; do
  if ! grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt"; then
    echo "ERROR: SR8A missing required stable-chain symbol: ${symbol}"
    exit 1
  fi
done

for marker in \
  'SR8A START receiver-only ShellUI bootstrap' \
  'SR8A NO PUI / NO Mono / NO UI hook / one injection only' \
  'SR8A INJECT attached=' \
  'SR8A HEALTH receiver READY' \
  'SR8A RECEIVER PASS; begin production PHUF lifecycle'; do
  if ! strings "${ELF}" | grep -Fq "${marker}"; then
    echo "ERROR: SR8A marker missing: ${marker}"
    exit 1
  fi
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
