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

echo "[1/9] Marking helper scripts executable"
chmod +x "$SCRIPT_DIR/launch_kiosk.sh"

echo "[2/9] Installing systemd unit for static dash server (repo=$REPO_ROOT)"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$SERVICE_SRC" | sudo tee "$SERVICE_DST" >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable --now bevo_dash_serve.service

echo "[3/9] Installing bevo_telemetry systemd unit (installed, NOT enabled)"
if [ -f "$TELEMETRY_SRC" ]; then
    sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$TELEMETRY_SRC" | sudo tee "$TELEMETRY_DST" >/dev/null
    sudo systemctl daemon-reload
    echo "  installed. Enable when prereqs (Python venv at \$REPO/.venv,"
    echo "  cellular modem, can0 bus) are ready:"
    echo "    sudo systemctl enable --now bevo_telemetry.service"
else
    echo "  (telemetry unit source not found at $TELEMETRY_SRC, skipping)"
fi

echo "[4/9] Installing Chromium-kiosk autostart entry"
mkdir -p "$HOME/.config/autostart"
sed "s|__BEVO_REPO__|$REPO_ROOT|g" "$DESKTOP_SRC" > "$DESKTOP_DST"

echo "[5/9] Overriding labwc system autostart (no panel, no desktop)"
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

echo "[6/9] Installing Plymouth boot-splash theme (black)"
if command -v plymouth-set-default-theme >/dev/null 2>&1; then
    sudo mkdir -p /usr/share/plymouth/themes/bevo
    sudo cp "$SCRIPT_DIR/plymouth/bevo/"* /usr/share/plymouth/themes/bevo/
    sudo plymouth-set-default-theme -R bevo
    echo "  bevo theme installed and set as default (initramfs rebuilt)"
else
    echo "  (plymouth-set-default-theme not present, skipping)"
fi

echo "[7/9] Verifying static server is up"
sleep 1
if curl -sfo /dev/null http://localhost:8080; then
    echo "  ok — http://localhost:8080 responds"
else
    echo "  WARNING: http://localhost:8080 did not respond yet."
    echo "  Check:  sudo systemctl status bevo_dash_serve.service"
    echo "  Did you run 'npm run build' in BEVO/dashd/frontend?"
fi

# Locus Lock (ChudPi) sits on a private ethernet subnet behind eth0.
# Static 192.168.1.50/24 with ipv4.never-default so cellular usb0 keeps
# the default route (MQTT + Tailscale stay over cell). Idempotent: check
# for the named connection first. Pi OS Trixie uses NetworkManager
# natively — no netplan file involved (Terence's guide is outdated on
# that point).
echo "[8/9] Configuring eth0 static IP for Locus Lock (bevo-locuslock)"
if ! nmcli -t -f NAME conn show 2>/dev/null | grep -qx 'bevo-locuslock'; then
    sudo nmcli conn add type ethernet ifname eth0 con-name bevo-locuslock \
        ipv4.method manual \
        ipv4.addresses 192.168.1.50/24 \
        ipv4.never-default true \
        ipv6.method disabled >/dev/null
    sudo nmcli conn up bevo-locuslock >/dev/null || \
        echo "  warning: bevo-locuslock add succeeded but bring-up failed (eth0 down?)"
    echo "  created and activated bevo-locuslock"
else
    echo "  bevo-locuslock already exists"
fi

# Rev B routes Locus Lock power through GPIO14 (BCM 14 = UART0 TXD by
# default). The kernel serial console claims that pin even without an
# active getty, which blocks pinctrl from holding it HIGH. Strip the
# serial console from cmdline.txt and disable the getty so locus_power.sh
# can drive the rail. A reboot is required for cmdline.txt changes to
# take effect.
echo "[9/9] Disabling serial console (frees GPIO14 for Locus Lock power)"
CMDLINE=/boot/firmware/cmdline.txt
NEEDS_REBOOT=0
if [ -f "$CMDLINE" ] && grep -qE 'console=serial0,[0-9]+' "$CMDLINE"; then
    sudo cp "$CMDLINE" "${CMDLINE}.bak.$(date +%Y%m%d_%H%M%S)"
    sudo sed -i -E 's/console=serial0,[0-9]+\s*//' "$CMDLINE"
    echo "  removed console=serial0 from cmdline.txt (backup saved)"
    NEEDS_REBOOT=1
else
    echo "  cmdline.txt already free of console=serial0"
fi
if systemctl is-enabled serial-getty@ttyAMA10.service >/dev/null 2>&1; then
    sudo systemctl disable --now serial-getty@ttyAMA10.service
    echo "  disabled serial-getty@ttyAMA10"
fi
if ! command -v pinctrl >/dev/null 2>&1; then
    echo "  WARNING: pinctrl not found — install raspi-utils for locus_power.sh"
fi

echo
if [ "$NEEDS_REBOOT" = "1" ]; then
    echo "Done. REBOOT REQUIRED to free GPIO14 (cmdline.txt changed)."
else
    echo "Done. Reboot or log out / back in to start the kiosk."
fi
echo "Manual kiosk launch (for debugging):"
echo "  $SCRIPT_DIR/launch_kiosk.sh"
