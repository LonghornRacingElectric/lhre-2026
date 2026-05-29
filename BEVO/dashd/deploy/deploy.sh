#!/usr/bin/env bash
# Update + redeploy the dash on the Pi.
#
# What it does, in order:
#   1. git pull on the repo
#   2. cargo build --release in BEVO/ (rebuilds cand / dashd / loggerd / publishd)
#   3. npm run build in BEVO/dashd/frontend (refreshes the static bundle)
#   4. Restart bevo_dash_serve.service and bevo_telemetry.service
#
# Run this from the Pi after committing/pushing changes from your dev machine.
# Idempotent — safe to re-run.
#
# Does NOT run `BEVO/nonhermetic/sync_assets.sh`. If you changed the proto
# schema, run that manually first; it needs bazel or protoc available.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DASHD_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BEVO_ROOT="$(cd "$DASHD_DIR/.." && pwd)"
REPO_ROOT="$(cd "$BEVO_ROOT/.." && pwd)"
FRONTEND_DIR="$DASHD_DIR/frontend"

echo "[1/4] git pull"
cd "$REPO_ROOT"
git pull --ff-only

echo "[2/4] cargo build --release (BEVO daemons)"
cd "$BEVO_ROOT"
cargo build --release

echo "[3/4] npm run build (frontend)"
cd "$FRONTEND_DIR"
if [[ ! -d node_modules ]]; then
    echo "  node_modules missing — running 'npm install' first"
    npm install
fi
npm run build

echo "[4/4] systemctl restart"
sudo systemctl restart bevo_dash_serve.service || true
if systemctl is-enabled --quiet bevo_telemetry.service 2>/dev/null; then
    sudo systemctl restart bevo_telemetry.service
fi

echo
echo "Done. To pick up new frontend bits in the running kiosk:"
echo "  - Click the dash and press Ctrl+R, or"
echo "  - pkill chromium  &&  $SCRIPT_DIR/launch_kiosk.sh"
