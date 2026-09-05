#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
export COMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE:-${ROOT}/.deps/etahen}"

python3 "${ROOT}/tests/test_plugin_wrapper.py"

# This CMake entry point is the canonical source-built PS5 target.
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${ROOT}/build-ps5" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE}"

cmake --build "${ROOT}/build-ps5" -j"$(nproc)"

mkdir -p "${ROOT}/dist"

ELF="${ROOT}/dist/Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf"
PLUGIN="${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.plugin"

test -f "${ELF}"
test -f "${PLUGIN}"

python3 "${ROOT}/tools/verify_test20_repro.py" "${ELF}" "${PLUGIN}"

sha256sum \
  "${ELF}" \
  "${PLUGIN}" \
  > "${ROOT}/dist/SHA256SUMS.txt"

cat "${ROOT}/dist/SHA256SUMS.txt"
