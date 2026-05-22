#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
BIN_DIR="$BEVO_ROOT/target/release"
CAN_JSON_PATH="$BEVO_ROOT/nonhermetic/assets/can.json"

# environment variables mirrored from run_real_stack.sh but for mock use
CAN_IFACE="${CAND_CAN_INTERFACE:-can0}"

cleanup() {
  kill "${MOCK_PID:-}" "${MOCK_MAIN_PID:-}" "${DEBUGD_PID:-}" "${DASHD_PID:-}" "${LOGGERD_PID:-}" "${PUBLISHD_PID:-}" >/dev/null 2>&1 || true
  rm -f /tmp/BEVO_publishd_ready /tmp/BEVO_cand.sock /tmp/BEVO_cand_publishd.sock
}
trap cleanup EXIT INT TERM

for bin in "$BIN_DIR/mock_main" "$BIN_DIR/dashd" "$BIN_DIR/loggerd" "$BIN_DIR/publishd" "$BIN_DIR/debugd"; do
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

# start the mock generator like run_mock_stack
python3 "$BEVO_ROOT/cand/mock_can.py" &
MOCK_PID=$!

# bring up publishd first so downstream components can connect.
# For full-mock local testing, default to waiting for server packet_id so
# publishd exercises the same handshake path as telemetry integration tests.
PUBLISHD_REQUIRE_SERVER_PACKET_ID="${PUBLISHD_REQUIRE_SERVER_PACKET_ID:-1}" "$BIN_DIR/publishd" &
PUBLISHD_PID=$!

# launch the remaining daemons
"$BIN_DIR/debugd" &
DEBUGD_PID=$!

"$BIN_DIR/dashd" &
DASHD_PID=$!

"$BIN_DIR/loggerd" &
LOGGERD_PID=$!

# mock_main publishes directly to the cand IPC sockets
CAND_CAN_JSON_PATH="$CAN_JSON_PATH" CAND_MOCK_PUBLISH_TARGETS="cand,publishd" "$BIN_DIR/mock_main" &
MOCK_MAIN_PID=$!

# wait on dashd as the primary process
wait "$DASHD_PID"
