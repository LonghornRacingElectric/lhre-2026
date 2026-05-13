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
LABWC_AUTOSTART_SRC="$SCRIPT_DIR/labwc-autostart"
LABWC_AUTOSTART_DST="$HOME/.config/labwc/autostart"

echo "[1/6] Marking helper scripts executable"
chmod +x "$SCRIPT_DIR/launch_kiosk.sh"

echo "[2/6] Installing systemd unit for static dash server (repo=$REPO_ROOT)"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$SERVICE_SRC" | sudo tee "$SERVICE_DST" >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable --now bevo_dash_serve.service

echo "[3/6] Installing Chromium-kiosk autostart entry"
mkdir -p "$HOME/.config/autostart"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$DESKTOP_SRC" > "$DESKTOP_DST"

echo "[4/6] Installing labwc user-level autostart (no panel, no desktop)"
mkdir -p "$HOME/.config/labwc"
cp "$LABWC_AUTOSTART_SRC" "$LABWC_AUTOSTART_DST"
chmod +x "$LABWC_AUTOSTART_DST"

echo "[5/6] Installing Plymouth boot-splash theme (black)"
if command -v plymouth-set-default-theme >/dev/null 2>&1; then
    sudo mkdir -p /usr/share/plymouth/themes/bevo
    sudo cp "$SCRIPT_DIR/plymouth/bevo/"* /usr/share/plymouth/themes/bevo/
    sudo plymouth-set-default-theme -R bevo
    echo "  bevo theme installed and set as default (initramfs rebuilt)"
else
    echo "  (plymouth-set-default-theme not present, skipping)"
fi

echo "[6/6] Verifying static server is up"
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
