#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
BIN_DIR="$BEVO_ROOT/target/release"
CAN_JSON_PATH="$BEVO_ROOT/nonhermetic/assets/can.json"
CAN_IFACE_0="${CAND_CAN_INTERFACE_0:-can0}"
CAN_IFACE_1="${CAND_CAN_INTERFACE_1:-can1}"
LOGGERD_ENABLED="${LOGGERD_ENABLED:-1}"

cleanup() {
  kill "${CAND_PID:-}" "${DASHD_PID:-}" "${PUBLISHD_PID:-}" "${LOGGERD_PID:-}" >/dev/null 2>&1 || true
  rm -f /tmp/BEVO_publishd_ready /tmp/BEVO_cand.sock /tmp/BEVO_cand_publishd.sock
}
trap cleanup EXIT INT TERM

for bin in "$BIN_DIR/cand" "$BIN_DIR/dashd" "$BIN_DIR/loggerd" "$BIN_DIR/publishd"; do
  if [[ ! -x "$bin" ]]; then
    echo "Missing binary: $bin" >&2
    echo "Run BEVO/nonhermetic/setup_local_env.sh first." >&2
    exit 1
  fi
done

if [[ ! -f "$CAN_JSON_PATH" ]]; then
  echo "Missing CAN json: $CAN_JSON_PATH" >&2
  echo "Run BEVO/nonhermetic/setup_local_env.sh first." >&2
  exit 1
fi

PUBLISHD_REQUIRE_SERVER_PACKET_ID="${PUBLISHD_REQUIRE_SERVER_PACKET_ID:-1}" "$BIN_DIR/publishd" &
PUBLISHD_PID=$!

"$BIN_DIR/dashd" &
DASHD_PID=$!

if [[ "$LOGGERD_ENABLED" == "1" ]]; then
  "$BIN_DIR/loggerd" &
  LOGGERD_PID=$!
fi

CAND_USE_MOCK=0 \
CAND_CAN_INTERFACE_0="$CAN_IFACE_0" \
CAND_CAN_INTERFACE_1="$CAN_IFACE_1" \
CAND_CAN_JSON_PATH="$CAN_JSON_PATH" "$BIN_DIR/cand" &
CAND_PID=$!

wait "$DASHD_PID"
