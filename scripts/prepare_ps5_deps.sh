#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="${ROOT}/.deps"
SDK_ZIP="/tmp/ps5-payload-sdk-v0.41.zip"

mkdir -p "${DEPS}"

echo "[1/4] PS5 Payload SDK v0.41"
wget -q \
  https://github.com/ps5-payload-dev/sdk/releases/download/v0.41/ps5-payload-sdk.zip \
  -O "${SDK_ZIP}"

sudo rm -rf /opt/ps5-payload-sdk
sudo unzip -q "${SDK_ZIP}" -d /opt

export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

test -x "${PS5_PAYLOAD_SDK}/bin/prospero-cmake"

echo "[2/4] etaHEN 2.5B source"
rm -rf "${DEPS}/etahen"
git clone -q https://github.com/etaHEN/etaHEN.git "${DEPS}/etahen"
git -C "${DEPS}/etahen" checkout -q \
  5061a85312eb1d2da811269bcb9061e4b01302a1

echo "[3/4] ps5-payload-dev/shsrv v0.20 source"
rm -rf "${DEPS}/shsrv"
git clone -q --branch v0.20 --depth 1 \
  https://github.com/ps5-payload-dev/shsrv.git "${DEPS}/shsrv"

echo "[4/4] Record exact resolved revisions"
{
  echo "PS5 Payload SDK: v0.41"
  echo "etaHEN: $(git -C "${DEPS}/etahen" rev-parse HEAD)"
  echo "shsrv: $(git -C "${DEPS}/shsrv" rev-parse HEAD)"
} > "${ROOT}/RESOLVED_BUILD_DEPENDENCIES.txt"

cat "${ROOT}/RESOLVED_BUILD_DEPENDENCIES.txt"
