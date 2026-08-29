#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"

python3 "${ROOT}/tests/test_plugin_wrapper.py"

mkdir -p "${ROOT}/dist"
rm -f "${ROOT}/dist/Common_FPS_RC12_Discovery_Only.elf" \
      "${ROOT}/dist/Common_FPS_RC12_Discovery_Only_etaHEN.plugin"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${ROOT}/build-ps5" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "${ROOT}/build-ps5" -j"$(nproc)"

test -f "${ROOT}/dist/Common_FPS_RC12_Discovery_Only.elf"
test -f "${ROOT}/dist/Common_FPS_RC12_Discovery_Only_etaHEN.plugin"

if strings "${ROOT}/dist/Common_FPS_RC12_Discovery_Only.elf" | grep -Eq 'pt_attach|pt_copyout|pt_detach'; then
  echo "ERROR: RC12 unexpectedly contains shsrv ptrace symbols"
  exit 1
fi

sha256sum \
  "${ROOT}/dist/Common_FPS_RC12_Discovery_Only.elf" \
  "${ROOT}/dist/Common_FPS_RC12_Discovery_Only_etaHEN.plugin" \
  > "${ROOT}/dist/SHA256SUMS.txt"

cat "${ROOT}/dist/SHA256SUMS.txt"
