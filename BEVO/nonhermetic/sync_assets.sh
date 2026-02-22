#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
REPO_ROOT="$(cd "$BEVO_ROOT/.." && pwd)"

CAN_PACKETS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_packets.csv"
CAN_BITFIELDS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_bitfields.csv"
CAN_JSON_OUT="$REPO_ROOT/drivers/longhorn-lib/can.json"
CAN_JSON_GEN_SCRIPT="$REPO_ROOT/drivers/longhorn-lib/scripts/generate_can_json.py"
PROTO_FILE="$REPO_ROOT/drivers/longhorn-lib/protobuf/can_packets.proto"
DESC_FILE="$BEVO_ROOT/sensor_data.desc"
CODEGEN_SCRIPT="$BEVO_ROOT/codegen.py"
MAPPING_OUT="$BEVO_ROOT/generated_mapping.rs"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found" >&2
  exit 1
fi

echo "[1/3] Generating CAN json"
python3 "$CAN_JSON_GEN_SCRIPT" "$CAN_PACKETS_CSV" "$CAN_BITFIELDS_CSV" "$CAN_JSON_OUT"

echo "[2/3] Generating descriptor"
bazel run @protobuf//:protoc -- \
  --descriptor_set_out="$DESC_FILE" \
  --proto_path="$REPO_ROOT" \
  "$PROTO_FILE"

echo "[3/3] Generating Rust mapping"
python3 "$CODEGEN_SCRIPT" \
  --json "$CAN_JSON_OUT" \
  --desc "$DESC_FILE" \
  --out "$MAPPING_OUT" \
  --message OrionSensorData

echo "Done. Synced: $CAN_JSON_OUT, $DESC_FILE, $MAPPING_OUT"
