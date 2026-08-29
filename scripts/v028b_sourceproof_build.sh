#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
export COMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE:-${ROOT}/.deps/etahen}"
export COMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE:-${ROOT}/.deps/shsrv}"

BUILD="${ROOT}/build-v028b-sourceproof"
DIST="${ROOT}/dist/v028b-sourceproof"
rm -rf "${BUILD}" "${DIST}"
mkdir -p "${DIST}"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE}"

cmake --build "${BUILD}" \
  --target common_fps_v028b_backend common_fps_v028b_bootstrap \
  -j"$(nproc)"

BACKEND="$(find "${BUILD}" -type f -name 'libCommon_FPS_v028b_backend_sourceproof.a' -print -quit)"
BOOTSTRAP="$(find "${BUILD}" -type f -name 'libCommon_FPS_v028b_bootstrap_sourceproof.a' -print -quit)"

if [[ -z "${BACKEND}" || ! -f "${BACKEND}" ]]; then
  echo "ERROR: v0.28b backend source-proof library was not produced"
  exit 1
fi
if [[ -z "${BOOTSTRAP}" || ! -f "${BOOTSTRAP}" ]]; then
  echo "ERROR: v0.28b bootstrap source-proof library was not produced"
  exit 1
fi

cp "${BACKEND}" "${DIST}/Common_FPS_v028b_backend_sourceproof.a"
cp "${BOOTSTRAP}" "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a"

# Producer invariant: the periodic FPS transport must never regress to the
# RC9-RC13 ptrace/MDBG remote-read path.
if nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | \
   grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout'; then
  echo "ERROR: forbidden ptrace/MDBG dependency in v0.28b backend"
  exit 1
fi

nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | grep -q 'kernel_copyout'
nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | grep -q 'kernel_dynlib_handle'
nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | grep -q 'kernel_dynlib_mapbase_addr'

# Bootstrap invariant: ptrace is expected here, but only in this isolated
# one-time startup library. Verify the recovered entry points are present.
nm "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" | grep -q 'inject_renderer_once'
nm "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" | grep -q 'commonfps_v028b_stager'
nm "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" | grep -q 'commonfps_v028b_elfldr_load'
nm "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" | grep -q 'commonfps_v028b_elfldr_payload_args'
nm "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" | grep -q 'pt_attach'
nm "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" | grep -q 'pt_detach'

sha256sum \
  "${DIST}/Common_FPS_v028b_backend_sourceproof.a" \
  "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a" \
  > "${DIST}/SHA256SUMS.txt"

{
  echo '=== BACKEND undefined symbols ==='
  nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a"
  echo
  echo '=== BOOTSTRAP undefined symbols ==='
  nm -u "${DIST}/Common_FPS_v028b_bootstrap_sourceproof.a"
} > "${DIST}/UNDEFINED_SYMBOLS.txt"

cat "${DIST}/SHA256SUMS.txt"
cat "${DIST}/UNDEFINED_SYMBOLS.txt"
