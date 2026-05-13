#!/usr/bin/env bash
# Install the static-serve systemd unit + Chromium-kiosk autostart entry.
# Idempotent: safe to re-run after pulling new code.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
SERVICE_SRC="$SCRIPT_DIR/bevo_dash_serve.service"
SERVICE_DST="/etc/systemd/system/bevo_dash_serve.service"
DESKTOP_SRC="$SCRIPT_DIR/dash-kiosk.desktop"
DESKTOP_DST="$HOME/.config/autostart/dash-kiosk.desktop"

echo "[1/4] Marking helper scripts executable"
chmod +x "$SCRIPT_DIR/launch_kiosk.sh"

echo "[2/4] Installing systemd unit for static dash server (repo=$REPO_ROOT)"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$SERVICE_SRC" | sudo tee "$SERVICE_DST" >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable --now bevo_dash_serve.service

echo "[3/4] Installing Chromium-kiosk autostart entry"
mkdir -p "$HOME/.config/autostart"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$DESKTOP_SRC" > "$DESKTOP_DST"

echo "[4/4] Verifying static server is up"
sleep 1
if curl -sfo /dev/null http://localhost:8080; then
    echo "  ok — http://localhost:8080 responds"
else
    echo "  WARNING: http://localhost:8080 did not respond yet."
    echo "  Check:  sudo systemctl status bevo_dash_serve.service"
    echo "  Did you run 'npm run build' in BEVO/dashd/frontend?"
fi

echo
echo "Done. Reboot or log out / back in to start the kiosk."
echo "Manual kiosk launch (for debugging):"
echo "  $SCRIPT_DIR/launch_kiosk.sh"
