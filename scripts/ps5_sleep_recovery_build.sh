#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
export COMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE:-${ROOT}/.deps/etahen}"
export COMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE:-${ROOT}/.deps/shsrv}"

python3 "${ROOT}/tests/test_plugin_wrapper.py"

"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" \
  -S "${ROOT}/ps5" \
  -B "${ROOT}/build-ps5-sleep-recovery" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMMON_FPS_SLEEP_RECOVERY=ON \
  -DCOMMON_FPS_ETAHEN_SOURCE="${COMMON_FPS_ETAHEN_SOURCE}" \
  -DCOMMON_FPS_SHSRV_SOURCE="${COMMON_FPS_SHSRV_SOURCE}"

cmake --build "${ROOT}/build-ps5-sleep-recovery" -j"$(nproc)"

mkdir -p "${ROOT}/dist"

ELF="${ROOT}/dist/Common_FPS_PS5_v1.1.1_SLEEP_RECOVERY.elf"
PLUGIN="${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.1_SLEEP_RECOVERY.plugin"
RENDERER="${ROOT}/dist/Common_FPS_ShellUI_v1.1.1_sleep_recovery.elf"

test -f "${ELF}"
test -f "${PLUGIN}"
test -f "${RENDERER}"

python3 "${ROOT}/tools/verify_v1_1_1_sleep_recovery_artifact.py" \
  "${ELF}" "${PLUGIN}"

if objdump -d --disassemble=main "${ELF}" | grep -q '<fork>'; then
  echo "ERROR: main still calls fork" >&2
  exit 1
fi

(
  cd "${ROOT}/dist"
  sha256sum \
    "$(basename "${ELF}")" \
    "$(basename "${PLUGIN}")" \
    "$(basename "${RENDERER}")"
) > "${ROOT}/dist/SHA256SUMS_v1.1.1_SLEEP_RECOVERY.txt"

cat "${ROOT}/dist/SHA256SUMS_v1.1.1_SLEEP_RECOVERY.txt"
