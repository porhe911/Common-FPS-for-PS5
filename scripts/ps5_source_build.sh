#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
export COMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE:-${ROOT}/.deps/etahen}"
export COMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE:-${ROOT}/.deps/shsrv}"

python3 "${ROOT}/tools/check_v1_source_gate.py"
python3 "${ROOT}/tests/test_plugin_wrapper.py"

# Do not let a stale pre-embedded companion look like a public artifact.
mkdir -p "${ROOT}/dist"
rm -f "${ROOT}/dist/Common_FPS_ShellUI_v1.1.0.elf"

# This CMake entry point is the canonical source-built PS5 target.
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${ROOT}/build-ps5" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE}"

cmake --build "${ROOT}/build-ps5" -j"$(nproc)"

test -f "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf"
test -f "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin"
test -f "${ROOT}/build-ps5/Common_FPS_ShellUI_v1.1.0.elf"
test ! -e "${ROOT}/dist/Common_FPS_ShellUI_v1.1.0.elf"

# Production/self-contained builds must retain the exact embedded renderer.
# RC4 is intentionally a sysctl-only safety probe: its main path never
# references the renderer, so --gc-sections is expected to discard that blob.
# Do not turn that expected diagnostic property into a false CI failure.
if grep -q "RC4 START sysctl-only" "${ROOT}/src/ps5/commonfps_ps5_main.cpp"; then
  echo "RC4 diagnostic build: embedded-renderer retention check intentionally skipped"
else
  python3 "${ROOT}/tools/verify_embedded_renderer.py" \
    --controller "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf" \
    --plugin "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin" \
    --renderer "${ROOT}/build-ps5/Common_FPS_ShellUI_v1.1.0.elf"
fi

sha256sum \
  "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf" \
  "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin" \
  > "${ROOT}/dist/SHA256SUMS.txt"

cat "${ROOT}/dist/SHA256SUMS.txt"
