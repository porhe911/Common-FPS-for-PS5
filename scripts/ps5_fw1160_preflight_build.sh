#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S "${ROOT}/ps5" \
  -B "${ROOT}/build-ps5-preflight" -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${ROOT}/.deps/etahen" \
  -DCOMMON_FPS_SHSRV_SOURCE="${ROOT}/.deps/shsrv"
cmake --build "${ROOT}/build-ps5-preflight" --target common_fps_fw1160_preflight -j2
python3 "${ROOT}/tools/verify_fw1160_preflight.py" \
  "${ROOT}/dist/Common_FPS_PS5_FW1160_Preflight_v0.1.elf" \
  "${ROOT}/dist/Common_FPS_PS5_etaHEN_FW1160_Preflight_v0.1.plugin"
(
  cd "${ROOT}/dist"
  sha256sum Common_FPS_PS5_FW1160_Preflight_v0.1.elf \
    Common_FPS_PS5_etaHEN_FW1160_Preflight_v0.1.plugin
) > "${ROOT}/dist/FW1160_PREFLIGHT_SHA256SUMS.txt"
