#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"

ETAHEN_SRC="${ROOT}/.deps/etahen"
SHSRV_SRC="${ROOT}/.deps/shsrv"
OUT="${ROOT}/dist/v028b-sr1"

rm -rf "${ROOT}/build-v028b-sr1"
mkdir -p "${OUT}"
rm -f "${OUT}/Common_FPS_SR1_v028b_Backend.elf" \
      "${OUT}/Common_FPS_SR1_v028b_Backend_etaHEN.plugin" \
      "${OUT}/SHA256SUMS.txt" \
      "${OUT}/UNDEFINED_SYMBOLS.txt"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${ROOT}/build-v028b-sr1" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${SHSRV_SRC}"

cmake --build "${ROOT}/build-v028b-sr1" \
  --target common_fps_v028b_sr1 -j"$(nproc)"

test -f "${OUT}/Common_FPS_SR1_v028b_Backend.elf"
test -f "${OUT}/Common_FPS_SR1_v028b_Backend_etaHEN.plugin"

# Hard safety invariant for SR1: none of the failed RC remote-inspection
# transports or renderer/injection symbols may be present.
if strings "${OUT}/Common_FPS_SR1_v028b_Backend.elf" | \
   grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout|set_ucred_to_debugger|SceShellUI|elfldr'; then
  echo "ERROR: SR1 contains a forbidden remote-inspection or renderer symbol"
  exit 1
fi

nm -u "${OUT}/Common_FPS_SR1_v028b_Backend.elf" > "${OUT}/UNDEFINED_SYMBOLS.txt" || true

# Required recovered-backend imports must remain visible in the final ELF.
for sym in kernel_copyout kernel_dynlib_handle kernel_dynlib_mapbase_addr sysctl; do
  if ! grep -q "${sym}" "${OUT}/UNDEFINED_SYMBOLS.txt"; then
    echo "ERROR: SR1 missing required recovered-backend import: ${sym}"
    exit 1
  fi
done

sha256sum \
  "${OUT}/Common_FPS_SR1_v028b_Backend.elf" \
  "${OUT}/Common_FPS_SR1_v028b_Backend_etaHEN.plugin" \
  > "${OUT}/SHA256SUMS.txt"

cat "${OUT}/SHA256SUMS.txt"
echo "--- undefined symbols ---"
cat "${OUT}/UNDEFINED_SYMBOLS.txt"
