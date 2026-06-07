#!/usr/bin/env bash
# Launch Chromium in kiosk mode pointed at the dash. Waits for the static
# server to come up before launching so we don't get a transient connection
# error on boot.
set -euo pipefail

URL="${BEVO_DASH_URL:-http://localhost:8080}"

# Configure the dash panel mode + rotation. Defaults match the BEVO panel:
# physically ~800x480 (advertises 800x600 but cuts ~120 rows; we synthesize
# 800x480 via --custom-mode), mounted upside-down. Override per-Pi via env.
DISPLAY_OUTPUT="${BEVO_DASH_OUTPUT:-HDMI-A-1}"
DISPLAY_MODE="${BEVO_DASH_MODE:-800x480}"
DISPLAY_TRANSFORM="${BEVO_DASH_TRANSFORM:-180}"
if command -v wlr-randr >/dev/null 2>&1; then
    wlr-randr --output "$DISPLAY_OUTPUT" \
        --custom-mode "$DISPLAY_MODE" \
        --transform "$DISPLAY_TRANSFORM" \
        || echo "[BEVO] wlr-randr failed; continuing with compositor defaults" >&2
fi

# Wait up to ~30 s for the static server to respond.
for _ in $(seq 1 30); do
    if curl -sfo /dev/null "$URL"; then
        break
    fi
    sleep 1
done

# Pi OS bookworm ships `chromium`, older releases `chromium-browser`.
# Flags worth flagging:
#   --incognito          — no disk cache or profile persists, so a frontend
#                          rebuild is picked up immediately. Without it,
#                          Chromium served a stale index.html from disk
#                          cache on driveday and we saw the old bundle.
#   --ozone-platform=wayland — labwc autostart inherits the right XDG env
#                          so this can be omitted there, but launching by
#                          hand from SSH (where DISPLAY/WAYLAND_DISPLAY
#                          weren't set up the same way) needs it explicit.
CHROMIUM_BIN=""
for bin in chromium-browser chromium; do
    if command -v "$bin" >/dev/null 2>&1; then
        CHROMIUM_BIN="$bin"
        break
    fi
done

if [[ -z "$CHROMIUM_BIN" ]]; then
    echo "[BEVO] No chromium binary found (tried: chromium-browser, chromium)" >&2
    exit 1
fi

# Supervise the kiosk: a renderer/GPU crash or OOM kill must NOT leave a blank
# dash for the rest of an endurance run. dashd and the static server already
# auto-restart via systemd, but the browser did not until this loop — XDG
# autostart launches it once and never again. Relaunch on exit with a short
# backoff. `|| true` keeps `set -e` from aborting the loop on a nonzero exit.
while true; do
    "$CHROMIUM_BIN" \
        --kiosk \
        --incognito \
        --ozone-platform=wayland \
        --noerrdialogs \
        --disable-restore-session-state \
        --disable-infobars \
        --disable-translate \
        --disable-features=TranslateUI \
        --check-for-update-interval=604800 \
        --overscroll-history-navigation=0 \
        --password-store=basic \
        "$URL" || true
    echo "[BEVO] Chromium kiosk exited; relaunching in 2s" >&2
    sleep 2
done
