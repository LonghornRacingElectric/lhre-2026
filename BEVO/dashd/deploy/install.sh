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
LABWC_AUTOSTART_DST="/etc/xdg/labwc/autostart"
TELEMETRY_SRC="$REPO_ROOT/BEVO/nonhermetic/bevo_telemetry.service"
TELEMETRY_DST="/etc/systemd/system/bevo_telemetry.service"

echo "[1/7] Marking helper scripts executable"
chmod +x "$SCRIPT_DIR/launch_kiosk.sh"

echo "[2/7] Installing systemd unit for static dash server (repo=$REPO_ROOT)"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$SERVICE_SRC" | sudo tee "$SERVICE_DST" >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable --now bevo_dash_serve.service

echo "[3/7] Installing bevo_telemetry systemd unit (installed, NOT enabled)"
if [ -f "$TELEMETRY_SRC" ]; then
    sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$TELEMETRY_SRC" | sudo tee "$TELEMETRY_DST" >/dev/null
    sudo systemctl daemon-reload
    echo "  installed. Enable when prereqs (Python venv at \$REPO/.venv,"
    echo "  cellular modem, can0 bus) are ready:"
    echo "    sudo systemctl enable --now bevo_telemetry.service"
else
    echo "  (telemetry unit source not found at $TELEMETRY_SRC, skipping)"
fi

echo "[4/7] Installing Chromium-kiosk autostart entry"
mkdir -p "$HOME/.config/autostart"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$DESKTOP_SRC" > "$DESKTOP_DST"

echo "[5/7] Overriding labwc system autostart (no panel, no desktop)"
if [ ! -f "${LABWC_AUTOSTART_DST}.bak" ]; then
    sudo cp "$LABWC_AUTOSTART_DST" "${LABWC_AUTOSTART_DST}.bak"
    echo "  backed up original to ${LABWC_AUTOSTART_DST}.bak"
fi
sudo cp "$LABWC_AUTOSTART_SRC" "$LABWC_AUTOSTART_DST"
sudo chmod +x "$LABWC_AUTOSTART_DST"
# Clean up the user-level autostart from a previous install (now redundant
# since Pi OS's labwc runs both user + system, causing duplicate kanshi).
# Back it up first in case anything's been customized locally.
if [ -f "$HOME/.config/labwc/autostart" ]; then
    cp "$HOME/.config/labwc/autostart" "$HOME/.config/labwc/autostart.bak"
    rm -f "$HOME/.config/labwc/autostart"
fi

echo "[6/7] Installing Plymouth boot-splash theme (black)"
if command -v plymouth-set-default-theme >/dev/null 2>&1; then
    sudo mkdir -p /usr/share/plymouth/themes/bevo
    sudo cp "$SCRIPT_DIR/plymouth/bevo/"* /usr/share/plymouth/themes/bevo/
    sudo plymouth-set-default-theme -R bevo
    echo "  bevo theme installed and set as default (initramfs rebuilt)"
else
    echo "  (plymouth-set-default-theme not present, skipping)"
fi

echo "[7/7] Verifying static server is up"
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
