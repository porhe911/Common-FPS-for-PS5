#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
ETAHEN_SRC="${ROOT}/.deps/etahen"
OUT="${ROOT}/dist/v028b-sr9j"
rm -rf "${ROOT}/build-v028b-sr9j"; mkdir -p "${OUT}"; rm -f "${OUT}"/*
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S "${ROOT}/ps5/sr9j" -B "${ROOT}/build-v028b-sr9j" -DCMAKE_BUILD_TYPE=Release -DCOMMON_FPS_ETAHEN_SOURCE="${ETAHEN_SRC}"
cmake --build "${ROOT}/build-v028b-sr9j" --target common_fps_v028b_sr9j -j"$(nproc)"
R="${OUT}/Common_FPS_SR9J_Label_Ctor_Queue_Receiver.elf"; E="${OUT}/Common_FPS_SR9J_v028b_Label_Ctor_Queue.elf"; P="${OUT}/Common_FPS_SR9J_v028b_Label_Ctor_Queue_etaHEN.plugin"
test -f "$R" && test -f "$E" && test -f "$P"
if strings "$R" | grep -Eqi 'AppendChild|RemoveFromParent|CreateUIFont|UIColor|TextColor|FitWidthToText|FitHeightToText|set_Text|set_X|set_Y|set_Name'; then echo "ERROR: SR9J receiver contains forbidden tree/property mutation code"; exit 1; fi
for m in START_LABEL_CTOR_QUEUE_PROOF OBJECT_ALLOCATED_UNINITIALIZED LABEL_CTOR_THEN_CLEAR_READY PASS_LABEL_CTOR_THROUGH_UI_QUEUE EnqueueEventAction ArrayList; do strings "$R" | grep -Fq "$m" || { echo "ERROR: marker missing $m"; exit 1; }; done
BAD="$(readelf -r "$R" | awk '/R_X86_64_/ {print $3}' | sort -u | grep -Ev '^(R_X86_64_RELATIVE|R_X86_64_GLOB_DAT|R_X86_64_64)$' || true)"; [[ -z "$BAD" ]] || { echo "$BAD"; exit 1; }
readelf -r "$R" | grep R_X86_64_GLOB_DAT > "${OUT}/RECEIVER_GLOB_DAT.txt" || true
nm -C "$E" > "${OUT}/LINKED_SYMBOLS.txt" || true
for m in inject_renderer_once commonfps_v028b_elfldr_load commonfps_v028b_import_unresolved_count pt_attach pt_detach find_process_pid_sysctl; do grep -q "$m" "${OUT}/LINKED_SYMBOLS.txt" || { echo "ERROR: controller symbol missing $m"; exit 1; }; done
sha256sum "$R" "$E" "$P" > "${OUT}/SHA256SUMS.txt"; cat "${OUT}/SHA256SUMS.txt"
