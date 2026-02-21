#!/usr/bin/env bash
set -euo pipefail

RUNFILES_ROOT="${RUNFILES_DIR:-$0.runfiles}"
CAND_BIN="$RUNFILES_ROOT/_main/BEVO/cand/cand"
DASHD_BIN="$RUNFILES_ROOT/_main/BEVO/dashd/dashd"
PUBLISHD_BIN="$RUNFILES_ROOT/_main/BEVO/publishd/publishd"

STARTUP_SEMAPHORE_PATH="/tmp/BEVO_publishd_ready"
IPC_SOCKET_PATH="/tmp/BEVO_cand.sock"

for bin in "$CAND_BIN" "$DASHD_BIN" "$PUBLISHD_BIN"; do
  if [[ ! -x "$bin" ]]; then
    echo "Could not find executable: $bin" >&2
    exit 1
  fi
done

cleanup() {
  kill "${CAND_PID:-}" "${DASHD_PID:-}" "${PUBLISHD_PID:-}" >/dev/null 2>&1 || true
  rm -f "$STARTUP_SEMAPHORE_PATH" "$IPC_SOCKET_PATH"
}
trap cleanup EXIT INT TERM

rm -f "$STARTUP_SEMAPHORE_PATH" "$IPC_SOCKET_PATH"

CAN_IFACE="${CAND_CAN_INTERFACE:-can0}"
echo "Starting physical CAN stack on interface: $CAN_IFACE"

"$PUBLISHD_BIN" &
PUBLISHD_PID=$!

"$DASHD_BIN" &
DASHD_PID=$!

CAND_USE_MOCK=0 CAND_CAN_INTERFACE="$CAN_IFACE" "$CAND_BIN" &
CAND_PID=$!

wait "$DASHD_PID"
