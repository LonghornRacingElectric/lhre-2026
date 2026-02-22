#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
REPO_ROOT="$(cd "$BEVO_ROOT/.." && pwd)"

ASSETS_DIR="$BEVO_ROOT/nonhermetic/assets"
PROTO_FILE="$ASSETS_DIR/can_packets.proto"
CAN_JSON_PATH="$ASSETS_DIR/can.json"

CAN_PACKETS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_packets.csv"
CAN_BITFIELDS_CSV="$REPO_ROOT/drivers/longhorn-lib/config/can_bitfields.csv"
CAN_JSON_GEN_SCRIPT="$REPO_ROOT/drivers/longhorn-lib/scripts/generate_can_json.py"

if ! command -v cargo >/dev/null 2>&1; then
  echo "cargo is required but not found" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found" >&2
  exit 1
fi

if [[ -f "$CAN_PACKETS_CSV" && -f "$CAN_BITFIELDS_CSV" && -f "$CAN_JSON_GEN_SCRIPT" ]]; then
  echo "Refreshing BEVO-local CAN json at $CAN_JSON_PATH from monorepo CSVs"
  python3 "$CAN_JSON_GEN_SCRIPT" "$CAN_PACKETS_CSV" "$CAN_BITFIELDS_CSV" "$CAN_JSON_PATH"
fi

if [[ ! -f "$PROTO_FILE" ]]; then
  echo "Missing required proto asset: $PROTO_FILE" >&2
  exit 1
fi

if [[ ! -f "$CAN_JSON_PATH" ]]; then
  echo "Missing required can.json asset: $CAN_JSON_PATH" >&2
  exit 1
fi

echo "Building BEVO local binaries with Cargo"
(
  PROTO_FILE="$PROTO_FILE" cargo build --release --manifest-path "$BEVO_ROOT/Cargo.toml"
)

echo "Done. Local nonhermetic BEVO build is ready."
