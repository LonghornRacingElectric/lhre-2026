#!/usr/bin/env bash
set -euo pipefail

RUNFILES_ROOT="${RUNFILES_DIR:-$0.runfiles}"
CAND_BIN="$RUNFILES_ROOT/_main/BEVO/cand/cand"
DASHD_BIN="$RUNFILES_ROOT/_main/BEVO/dashd/dashd"

for bin in "$CAND_BIN" "$DASHD_BIN"; do
  if [[ ! -x "$bin" ]]; then
    echo "Could not find executable: $bin" >&2
    exit 1
  fi
done

cleanup() {
  kill "${CAND_PID:-}" "${DASHD_PID:-}" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

CAN_IFACE="${CAND_CAN_INTERFACE:-can0}"
echo "Starting physical CAN stack on interface: $CAN_IFACE"

"$DASHD_BIN" &
DASHD_PID=$!

CAND_USE_MOCK=0 CAND_CAN_INTERFACE="$CAN_IFACE" "$CAND_BIN" &
CAND_PID=$!

wait "$DASHD_PID"
