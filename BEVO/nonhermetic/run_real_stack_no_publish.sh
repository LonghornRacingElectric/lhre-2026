#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
BIN_DIR="$BEVO_ROOT/target/release"
CAN_JSON_PATH="$BEVO_ROOT/nonhermetic/assets/can.json"
CAN_IFACE="${CAND_CAN_INTERFACE:-can0}"
LOGGERD_ENABLED="${LOGGERD_ENABLED:-1}"

cleanup() {
  kill "${CAND_PID:-}" "${DASHD_PID:-}" "${LOGGERD_PID:-}" >/dev/null 2>&1 || true
  rm -f /tmp/BEVO_publishd_ready /tmp/BEVO_cand.sock /tmp/BEVO_cand_publishd.sock
}
trap cleanup EXIT INT TERM

for bin in "$BIN_DIR/cand" "$BIN_DIR/dashd" "$BIN_DIR/loggerd"; do
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

"$BIN_DIR/dashd" &
DASHD_PID=$!

if [[ "$LOGGERD_ENABLED" == "1" ]]; then
  "$BIN_DIR/loggerd" &
  LOGGERD_PID=$!
fi

CAND_USE_MOCK=0 CAND_CAN_INTERFACE="$CAN_IFACE" CAND_CAN_JSON_PATH="$CAN_JSON_PATH" "$BIN_DIR/cand" &
CAND_PID=$!

wait "$DASHD_PID"
