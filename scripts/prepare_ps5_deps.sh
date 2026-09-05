#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="${ROOT}/.deps"
SDK_ZIP="/tmp/ps5-payload-sdk-v0.41.zip"

ETAHEN_VERSION="2.4B"
ETAHEN_COMMIT="d47f99bd37f349ae59b3c4b66e09e93ba69f56cd"

mkdir -p "${DEPS}"

echo "[1/3] PS5 Payload SDK v0.41"
wget -q \
  https://github.com/ps5-payload-dev/sdk/releases/download/v0.41/ps5-payload-sdk.zip \
  -O "${SDK_ZIP}"

sudo rm -rf /opt/ps5-payload-sdk
sudo unzip -q "${SDK_ZIP}" -d /opt

export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
test -x "${PS5_PAYLOAD_SDK}/bin/prospero-cmake"

echo "[2/3] etaHEN ${ETAHEN_VERSION} source (${ETAHEN_COMMIT})"
rm -rf "${DEPS}/etahen"
git clone -q https://github.com/etaHEN/etaHEN.git "${DEPS}/etahen"
git -C "${DEPS}/etahen" checkout -q "${ETAHEN_COMMIT}"

RESOLVED_ETAHEN="$(git -C "${DEPS}/etahen" rev-parse HEAD)"
if [ "${RESOLVED_ETAHEN}" != "${ETAHEN_COMMIT}" ]; then
  echo "ERROR: etaHEN revision mismatch"
  echo "Expected: ${ETAHEN_COMMIT}"
  echo "Actual:   ${RESOLVED_ETAHEN}"
  exit 1
fi

echo "[3/3] Record exact resolved revisions"
{
  echo "PS5 Payload SDK: v0.41"
  echo "etaHEN: ${RESOLVED_ETAHEN} (${ETAHEN_VERSION})"
} > "${ROOT}/RESOLVED_BUILD_DEPENDENCIES.txt"

cat "${ROOT}/RESOLVED_BUILD_DEPENDENCIES.txt"
