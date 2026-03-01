#!/usr/bin/env bash
set -euo pipefail

RUNFILES_ROOT="${RUNFILES_DIR:-$0.runfiles}"
CAND_BIN="$RUNFILES_ROOT/_main/BEVO/cand/cand"
MOCK_BIN="$RUNFILES_ROOT/_main/BEVO/cand/mock_can"
DASHD_BIN="$RUNFILES_ROOT/_main/BEVO/dashd/dashd"
LOGGERD_BIN="$RUNFILES_ROOT/_main/BEVO/loggerd/loggerd"
CAN_JSON_PATH="$RUNFILES_ROOT/_main/drivers/longhorn-lib/can.json"

STARTUP_SEMAPHORE_PATH="/tmp/BEVO_publishd_ready"
IPC_SOCKET_PATH="/tmp/BEVO_cand.sock"

for bin in "$CAND_BIN" "$MOCK_BIN" "$DASHD_BIN" "$LOGGERD_BIN"; do
  if [[ ! -x "$bin" ]]; then
    echo "Could not find executable: $bin" >&2
    exit 1
  fi
done

if [[ ! -f "$CAN_JSON_PATH" ]]; then
  echo "Could not find CAN json: $CAN_JSON_PATH" >&2
  exit 1
fi

cleanup() {
  kill "${MOCK_PID:-}" "${CAND_PID:-}" "${DASHD_PID:-}" "${LOGGERD_PID:-}" >/dev/null 2>&1 || true
  rm -f "$STARTUP_SEMAPHORE_PATH" "$IPC_SOCKET_PATH"
}
trap cleanup EXIT INT TERM

rm -f "$STARTUP_SEMAPHORE_PATH" "$IPC_SOCKET_PATH"

"$DASHD_BIN" &
DASHD_PID=$!

"$LOGGERD_BIN" &
LOGGERD_PID=$!

CAND_USE_MOCK=1 CAND_CAN_JSON_PATH="$CAN_JSON_PATH" "$CAND_BIN" &
CAND_PID=$!

"$MOCK_BIN" &
MOCK_PID=$!

wait "$DASHD_PID"
