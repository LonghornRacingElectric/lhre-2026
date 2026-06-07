#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
REPO_ROOT="$(cd "$BEVO_ROOT/.." && pwd)"
ASSETS_DIR="$BEVO_ROOT/nonhermetic/assets"

CAN_PACKETS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_packets.csv"
CAN_BITFIELDS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_bitfields.csv"
CAN_JSON_OUT="$ASSETS_DIR/can.json"
CAN_JSON_GEN_SCRIPT="$REPO_ROOT/drivers/longhorn-lib/scripts/generate_can_json.py"
CAN_PROTO_SRC="$REPO_ROOT/drivers/longhorn-lib/protobuf/can_packets.proto"
PROTO_FILE="$ASSETS_DIR/can_packets.proto"
DESC_FILE="$BEVO_ROOT/sensor_data.desc"
CODEGEN_SCRIPT="$BEVO_ROOT/codegen.py"
MAPPING_OUT="$BEVO_ROOT/generated_mapping.rs"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found" >&2
  exit 1
fi

if [[ -f "$CAN_PACKETS_CSV" && -f "$CAN_BITFIELDS_CSV" && -f "$CAN_JSON_GEN_SCRIPT" ]]; then
  echo "[1/3] Refreshing BEVO-local CAN json from monorepo CSVs"
  python3 "$CAN_JSON_GEN_SCRIPT" "$CAN_PACKETS_CSV" "$CAN_BITFIELDS_CSV" "$CAN_JSON_OUT"
else
  echo "[1/3] Using existing BEVO-local CAN json at $CAN_JSON_OUT"
fi

if [[ ! -f "$CAN_JSON_OUT" ]]; then
  echo "Missing required CAN json asset: $CAN_JSON_OUT" >&2
  exit 1
fi

if [[ -f "$CAN_PROTO_SRC" ]]; then
  echo "[2/4] Syncing BEVO proto from monorepo source"
  cp "$CAN_PROTO_SRC" "$PROTO_FILE"
else
  echo "[2/4] Using existing BEVO-local proto at $PROTO_FILE"
fi

if [[ ! -f "$PROTO_FILE" ]]; then
  echo "Missing required proto asset: $PROTO_FILE" >&2
  exit 1
fi

echo "[3/4] Generating descriptor"
# Prefer the system `protoc` when present — it's instantaneous (single binary).
# Fall back to `bazel run @protobuf//:protoc` only when there's no system
# protoc, so hermetic bazel builds still work. Previously the order was
# reversed: GitHub's ubuntu-latest runner has bazel preinstalled, so CI was
# going down the bazel path and rebuilding protoc from source on a cold
# external repo — ~6 min per schema-drift job. Override with USE_BAZEL_PROTOC=1
# to force the bazel path explicitly.
if [[ "${USE_BAZEL_PROTOC:-0}" != "1" ]] && command -v protoc >/dev/null 2>&1; then
  protoc \
    --descriptor_set_out="$DESC_FILE" \
    --proto_path="$BEVO_ROOT" \
    "$PROTO_FILE"
elif command -v bazel >/dev/null 2>&1; then
  bazel run @protobuf//:protoc -- \
    --descriptor_set_out="$DESC_FILE" \
    --proto_path="$BEVO_ROOT" \
    "$PROTO_FILE"
else
  echo "Need either 'protoc' or 'bazel' to generate descriptor" >&2
  exit 1
fi

echo "[4/4] Generating Rust mapping"
python3 "$CODEGEN_SCRIPT" \
  --json "$CAN_JSON_OUT" \
  --desc "$DESC_FILE" \
  --out "$MAPPING_OUT" \
  --message OrionSensorData

echo "Done. Synced: $CAN_JSON_OUT, $PROTO_FILE, $DESC_FILE, $MAPPING_OUT"
