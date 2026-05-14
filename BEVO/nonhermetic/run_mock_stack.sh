#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
BIN_DIR="$BEVO_ROOT/target/release"
CAN_JSON_PATH="$BEVO_ROOT/nonhermetic/assets/can.json"

cleanup() {
  kill "${MOCK_PID:-}" "${MOCK_MAIN_PID:-}" "${DEBUGD_PID:-}" >/dev/null 2>&1 || true
  rm -f /tmp/BEVO_publishd_ready /tmp/BEVO_cand.sock /tmp/BEVO_cand_publishd.sock
}
trap cleanup EXIT INT TERM

for bin in "$BIN_DIR/mock_main" "$BIN_DIR/debugd"; do
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

python3 "$BEVO_ROOT/cand/mock_can.py" &
MOCK_PID=$!

"$BIN_DIR/debugd" &
DEBUGD_PID=$!

CAND_CAN_JSON_PATH="$CAN_JSON_PATH" CAND_MOCK_PUBLISH_TARGETS=cand "$BIN_DIR/mock_main" &
MOCK_MAIN_PID=$!

wait "$DEBUGD_PID"
