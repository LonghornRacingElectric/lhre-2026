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

if [[ ! -f "$PROTO_FILE" ]]; then
  echo "Missing required proto asset: $PROTO_FILE" >&2
  exit 1
fi

echo "[2/3] Generating descriptor"
if command -v bazel >/dev/null 2>&1; then
  bazel run @protobuf//:protoc -- \
    --descriptor_set_out="$DESC_FILE" \
    --proto_path="$BEVO_ROOT" \
    "$PROTO_FILE"
elif command -v protoc >/dev/null 2>&1; then
  protoc \
    --descriptor_set_out="$DESC_FILE" \
    --proto_path="$BEVO_ROOT" \
    "$PROTO_FILE"
else
  echo "Need either 'bazel' or 'protoc' to generate descriptor" >&2
  exit 1
fi

echo "[3/3] Generating Rust mapping"
python3 "$CODEGEN_SCRIPT" \
  --json "$CAN_JSON_OUT" \
  --desc "$DESC_FILE" \
  --out "$MAPPING_OUT" \
  --message OrionSensorData

echo "Done. Synced: $CAN_JSON_OUT, $DESC_FILE, $MAPPING_OUT"
