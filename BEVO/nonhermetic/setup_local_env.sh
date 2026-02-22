#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
REPO_ROOT="$(cd "$BEVO_ROOT/.." && pwd)"

CAN_PACKETS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_packets.csv"
CAN_BITFIELDS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_bitfields.csv"
CAN_JSON_OUT="$REPO_ROOT/drivers/longhorn-lib/can.json"
CAN_JSON_GEN_SCRIPT="$REPO_ROOT/drivers/longhorn-lib/scripts/generate_can_json.py"

if ! command -v cargo >/dev/null 2>&1; then
  echo "cargo is required but not found" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found" >&2
  exit 1
fi

echo "Generating CAN json at $CAN_JSON_OUT"
python3 "$CAN_JSON_GEN_SCRIPT" "$CAN_PACKETS_CSV" "$CAN_BITFIELDS_CSV" "$CAN_JSON_OUT"

echo "Building BEVO local binaries with Cargo"
(
  cd "$BEVO_ROOT"
  PROTO_FILE="$REPO_ROOT/drivers/longhorn-lib/protobuf/can_packets.proto" cargo build --release
)

echo "Done. Local nonhermetic BEVO build is ready."
