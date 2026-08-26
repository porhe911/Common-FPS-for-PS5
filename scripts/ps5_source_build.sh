#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
export COMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE:-${ROOT}/.deps/etahen}"
export COMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE:-${ROOT}/.deps/shsrv}"

python3 "${ROOT}/tools/check_v1_source_gate.py"
python3 "${ROOT}/tests/test_plugin_wrapper.py"

# This CMake entry point is the canonical source-built PS5 target.
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${ROOT}/build-ps5" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE}"

cmake --build "${ROOT}/build-ps5" -j"$(nproc)"

mkdir -p "${ROOT}/dist"

test -f "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf"
test -f "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin"
test -f "${ROOT}/dist/Common_FPS_ShellUI_v1.1.0.elf"

sha256sum \
  "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf" \
  "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin" \
  "${ROOT}/dist/Common_FPS_ShellUI_v1.1.0.elf" \
  > "${ROOT}/dist/SHA256SUMS.txt"

cat "${ROOT}/dist/SHA256SUMS.txt"
