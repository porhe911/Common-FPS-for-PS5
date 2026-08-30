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

# Receiver must stay completely UI-free and carry the file heartbeat marker.
if strings "${RECEIVER}" | grep -Eqi 'Mono|PUI|CreateLabel|OnRender|SceShellUI|Detour'; then
  echo "ERROR: SR8A receiver unexpectedly contains UI/Mono/hook code"
  exit 1
fi
strings "${RECEIVER}" | grep -Fq '/data/CommonFPS_SR8A_receiver_health.log'

# The recovered stable loader handles the two relocation classes actually used
# by the embedded renderer/receiver. Reject any other dynamic relocation before
# the binary can ever be offered for hardware testing.
BAD_RELOCS="$(readelf -r "${RECEIVER}" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT)$' || true)"
if [[ -n "${BAD_RELOCS}" ]]; then
  echo "ERROR: SR8A receiver has unsupported relocations:"
  echo "${BAD_RELOCS}"
  exit 1
fi
GLOB_COUNT="$(readelf -r "${RECEIVER}" | grep -c R_X86_64_GLOB_DAT || true)"
if [[ "${GLOB_COUNT}" -le 0 ]]; then
  echo "ERROR: SR8A receiver unexpectedly has no GLOB_DAT imports"
  exit 1
fi
echo "SR8A receiver GLOB_DAT count=${GLOB_COUNT}"
readelf -r "${RECEIVER}" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt"

# Controller intentionally contains ptrace only for ONE ShellUI bootstrap.
# MDBG and the old Stage-B visual renderer must never enter this target.
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
  'commonfps_v028b_import_resolved_count' \
  'commonfps_v028b_import_unresolved_count' \
  'kernel_dynlib_dlsym' \
  'pt_attach' \
  'pt_detach' \
  'proc_read' \
  'resolve_videoout_counter'; do
  if ! grep -q "${symbol}" "${OUT}/LINKED_SYMBOLS.txt"; then
    echo "ERROR: SR8A missing required stable-chain symbol: ${symbol}"
    exit 1
  fi
done

# Exact stable import resolver library names recovered from v0.28b must survive
# into the controller ELF.
for lib in \
  'libkernel_sys.sprx' \
  'libkernel.sprx' \
  'libSceLibcInternal.sprx' \
  'libmonosgen-2.0.sprx' \
  'libmonosgen-2.0.0.sprx' \
  'libScePad.sprx' \
  'libSceUserService.sprx'; do
  strings "${ELF}" | grep -Fq "${lib}" || {
    echo "ERROR: SR8A stable resolver library missing: ${lib}"
    exit 1
  }
done

for marker in \
  'SR8A START fail-closed receiver-only ShellUI bootstrap' \
  'SR8A NO PUI / NO Mono / NO UI hook / one injection only' \
  'SR8A loader=libNineS+TraceContinue+PHU GLOB_DAT resolver' \
  'imports_resolved=' \
  'imports_unresolved=' \
  'SR8A RECEIVER READY PASS; begin production PHUF lifecycle'; do
  if ! strings "${ELF}" | grep -Fq "${marker}"; then
    echo "ERROR: SR8A marker missing: ${marker}"
    exit 1
  fi
done

sha256sum "${RECEIVER}" "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
