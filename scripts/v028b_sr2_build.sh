#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
SHSRV_SRC="${ROOT}/.deps/shsrv"
OUT="${ROOT}/dist/v028b-sr2"

rm -rf "${ROOT}/build-v028b-sr2"
mkdir -p "${OUT}"
rm -f "${OUT}/Common_FPS_SR2_v028b_Auth_DMAP.elf" \
      "${OUT}/Common_FPS_SR2_v028b_Auth_DMAP_etaHEN.plugin" \
      "${OUT}/SHA256SUMS.txt" "${OUT}/LINKED_SYMBOLS.txt"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" -B "${ROOT}/build-v028b-sr2" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${SHSRV_SRC}"

cmake --build "${ROOT}/build-v028b-sr2" --target common_fps_v028b_sr2 -j"$(nproc)"

ELF="${OUT}/Common_FPS_SR2_v028b_Auth_DMAP.elf"
PLUGIN="${OUT}/Common_FPS_SR2_v028b_Auth_DMAP_etaHEN.plugin"
test -f "${ELF}"
test -f "${PLUGIN}"

# SR2 must not regress to failed RC transports or renderer/injection code.
if strings "${ELF}" | grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout|SceShellUI|elfldr'; then
  echo "ERROR: SR2 contains forbidden ptrace/MDBG/renderer symbol"
  exit 1
fi

nm -C "${ELF}" > "${OUT}/LINKED_SYMBOLS.txt" || true
for sym in find_game_pid_sysctl translate proc_read resolve_videoout_counter read_videoout_counter; do
  if ! grep -q "${sym}" "${OUT}/LINKED_SYMBOLS.txt"; then
    echo "ERROR: SR2 missing recovered backend symbol: ${sym}"
    exit 1
  fi
done

# The direct self-ucred path and debugger Auth ID marker must be present.
if ! strings "${ELF}" | grep -q 'SR2 AUTH debugger active only for VideoOut discovery'; then
  echo "ERROR: SR2 auth-gate marker missing"
  exit 1
fi

sha256sum "${ELF}" "${PLUGIN}" > "${OUT}/SHA256SUMS.txt"
cat "${OUT}/SHA256SUMS.txt"
