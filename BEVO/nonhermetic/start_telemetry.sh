#!/usr/bin/env bash
set -euo pipefail

SCRIPT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BEVO_ROOT="$(cd "$SCRIPT_ROOT/.." && pwd)"
REPO_ROOT="$(cd "$BEVO_ROOT/.." && pwd)"

PYTHON_BIN="${BEVO_PYTHON_BIN:-$REPO_ROOT/.venv/bin/python}"
CELL_SCRIPT="${BEVO_CELL_SCRIPT:-$BEVO_ROOT/cell.py}"
CAN_IFACE="${CAND_CAN_INTERFACE:-can0}"
CAN_BITRATE="${BEVO_CAN_BITRATE:-1000000}"
OPENVPN_CONFIG="${BEVO_OPENVPN_CONFIG:-/etc/openvpn/client/client.ovpn}"

echo "Starting BEVO telemetry system..."
echo "Repo root: $REPO_ROOT"
echo "BEVO root: $BEVO_ROOT"

if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "Missing python executable at $PYTHON_BIN" >&2
  echo "Set BEVO_PYTHON_BIN or create the BEVO virtual environment first." >&2
  exit 1
fi

if [[ ! -f "$CELL_SCRIPT" ]]; then
  echo "Missing cellular control script: $CELL_SCRIPT" >&2
  exit 1
fi

echo "Turning on cellular module..."
"$PYTHON_BIN" "$CELL_SCRIPT" on
sleep 5

echo "Configuring CAN interface $CAN_IFACE @ $CAN_BITRATE bps..."
if ip link show "$CAN_IFACE" >/dev/null 2>&1; then
  ip link set "$CAN_IFACE" down >/dev/null 2>&1 || true
fi
ip link set "$CAN_IFACE" up type can bitrate "$CAN_BITRATE"

echo "Starting VPN connection..."
openvpn --config "$OPENVPN_CONFIG" --daemon
sleep 5

echo "Starting BEVO real CAN stack..."
exec "$SCRIPT_ROOT/run_real_stack.sh"
