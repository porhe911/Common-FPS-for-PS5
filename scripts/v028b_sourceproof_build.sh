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
  --target common_fps_v028b_backend \
  -j"$(nproc)"

LIB="$(find "${BUILD}" -type f -name 'libCommon_FPS_v028b_backend_sourceproof.a' -print -quit)"
if [[ -z "${LIB}" || ! -f "${LIB}" ]]; then
  echo "ERROR: v0.28b source-proof static library was not produced"
  exit 1
fi

cp "${LIB}" "${DIST}/Common_FPS_v028b_backend_sourceproof.a"

# The recovered stable producer transport must not regress to the RC9-RC13
# remote-read mechanisms.
if nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | \
   grep -Eq 'pt_attach|pt_copyout|pt_detach|mdbg_copyout'; then
  echo "ERROR: forbidden ptrace/MDBG dependency in v0.28b backend"
  exit 1
fi

# Positive source-contract checks.
nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | \
  grep -q 'kernel_copyout'
nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | \
  grep -q 'kernel_dynlib_handle'
nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" | \
  grep -q 'kernel_dynlib_mapbase_addr'

sha256sum "${DIST}/Common_FPS_v028b_backend_sourceproof.a" \
  > "${DIST}/SHA256SUMS.txt"

nm -u "${DIST}/Common_FPS_v028b_backend_sourceproof.a" \
  > "${DIST}/UNDEFINED_SYMBOLS.txt"

cat "${DIST}/SHA256SUMS.txt"
cat "${DIST}/UNDEFINED_SYMBOLS.txt"
