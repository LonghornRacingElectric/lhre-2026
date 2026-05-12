# BEVO dash deploy

Everything needed to get the React dash showing up on the Pi screen, plus the
update workflow once it's there.

## What's in this directory

| File                          | Where it lives on the Pi                            | Purpose                                                  |
|-------------------------------|------------------------------------------------------|----------------------------------------------------------|
| `bevo_dash_serve.service`     | `/etc/systemd/system/`                              | systemd unit; serves the React build over HTTP :8080    |
| `dash-kiosk.desktop`          | `~/.config/autostart/`                              | Auto-launches Chromium kiosk on graphical login          |
| `launch_kiosk.sh`             | runs in place                                       | Waits for the server, then `chromium --kiosk` at it      |
| `install.sh`                  | runs once                                           | Copies the two config files above, enables the unit     |
| `deploy.sh`                   | runs each update                                    | `git pull` + rebuild Rust/JS + restart services          |

## How the runtime fits together

On the Pi at boot:

1. `bevo_telemetry.service` (in `BEVO/nonhermetic/`) launches `cand` + `dashd` + `publishd` + `loggerd`.
2. `bevo_dash_serve.service` (this dir) runs `python3 -m http.server 8080` from the React build directory. Serves HTML/JS only.
3. After graphical login as `lhre`, `dash-kiosk.desktop` runs `launch_kiosk.sh`, which polls the static server and opens Chromium kiosk-mode at `http://localhost:8080`.
4. The page's JS connects to `ws://localhost:8001` to receive `DashMessage` JSON from `dashd` at 30 Hz.

Two ports: HTTP :8080 (static), WebSocket :8001 (data). Same-machine, no CORS concerns. Static server binds to `127.0.0.1` only — the dash UI is never exposed to the WiFi the Pi is on.

## First-time Pi setup

The Pi is assumed to be running Raspberry Pi OS (Bookworm or newer) with a desktop session, autologin enabled, and user `lhre`.

```bash
# System packages
sudo apt update
sudo apt install -y git build-essential pkg-config libssl-dev \
    python3 chromium-browser curl

# Rust toolchain (stable)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
. "$HOME/.cargo/env"

# Node + npm (use distro version; v18 or newer is fine for CRA build)
sudo apt install -y nodejs npm

# Clone repo (matches paths in bevo_telemetry.service)
mkdir -p ~/Documents
cd ~/Documents
git clone https://github.com/LonghornRacingElectric/lhre-2026.git
cd lhre-2026/BEVO

# Generate vendored CAN assets + Rust mapping (one-time)
bash nonhermetic/setup_local_env.sh

# Build the daemons
cargo build --release

# Build the frontend
cd dashd/frontend
npm install
npm run build
cd ../..

# Install the dash kiosk + static-serve unit
bash dashd/deploy/install.sh

# Enable the telemetry stack (cand + dashd + publishd + loggerd) on boot
sudo cp nonhermetic/bevo_telemetry.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now bevo_telemetry.service

# Reboot to verify the kiosk comes up clean
sudo reboot
```

Common installs that *might* be missing:

- `protoc` or `bazel` — required by `BEVO/nonhermetic/sync_assets.sh` when the `.proto` changes. `cargo build` itself does not need either of them; `BEVO/build.rs` uses `prost_build` directly on `nonhermetic/assets/can_packets.proto`. So most pulls don't need a sync. If you see a build error about `OrionSensorData` not having a field that's clearly in the proto, run sync_assets.

## Update workflow

Once first-time setup is done, the day-to-day cycle is:

```bash
bash ~/Documents/lhre-2026/BEVO/dashd/deploy/deploy.sh
```

That pulls, rebuilds Rust + JS, restarts both services. To pick up new frontend bits in the kiosk that's already running, hit Ctrl+R in the browser or:

```bash
pkill chromium
~/Documents/lhre-2026/BEVO/dashd/deploy/launch_kiosk.sh
```

## First-run checklist

- [ ] Pi auto-logs into a graphical session as `lhre` (raspi-config → System → Boot → To Desktop, autologin).
- [ ] `chromium-browser` or `chromium` is installed.
- [ ] `python3` is installed (default on Pi OS).
- [ ] `BEVO/dashd/frontend/build/index.html` exists (from `npm run build`).
- [ ] `bevo_telemetry.service` is enabled and active (`systemctl status bevo_telemetry`).
- [ ] `bevo_dash_serve.service` is enabled and active (`systemctl status bevo_dash_serve`).
- [ ] After reboot, Chromium opens full-screen with the dash visible.

## Diagnostics

### "Dash is blank or just shows --"

Most likely `dashd` isn't running, or `cand` isn't, or the CAN interface is down.

```bash
sudo systemctl status bevo_telemetry.service
sudo systemctl status bevo_dash_serve.service
journalctl -u bevo_telemetry.service -n 50
ip link show can0                       # should be UP at 1000000 bitrate
ss -tlnp | grep -E '8080|8001'          # both ports listening?
curl -s http://localhost:8080 | head    # static HTML
```

Note: `dashd` now nulls all CAN fields after 3 seconds without an IPC update from `cand`, so an all-`--` dash means cand is silent (or the IPC socket is broken), not that the screen is busted.

### "Connection failed in browser console"

The frontend JS hardcodes `ws://localhost:8001`. If `dashd` isn't running, the WebSocket fails. Check `systemctl status bevo_telemetry.service` and `ss -tlnp | grep 8001`.

### "Kiosk launches but Chrome's showing 'connection refused'"

The static server didn't come up. Either `npm run build` was never run (missing `build/index.html`), or the service crashed. `journalctl -u bevo_dash_serve.service -n 50`.

### "I see CRLF / bad-interpreter errors when running .sh"

Some Windows checkout brought CRLF line endings in despite `.gitattributes`. Fix in place:

```bash
sed -i 's/\r$//' ~/Documents/lhre-2026/BEVO/dashd/deploy/*.sh
```

### Manual control of components

| Action                            | Command                                                       |
|-----------------------------------|---------------------------------------------------------------|
| View dashd / cand / publishd logs | `journalctl -u bevo_telemetry.service -f`                     |
| View static-server logs           | `journalctl -u bevo_dash_serve.service -f`                    |
| Restart everything                | `sudo systemctl restart bevo_telemetry.service bevo_dash_serve.service` |
| Kill the kiosk                    | `pkill chromium`                                              |
| Relaunch the kiosk                | `bash ~/Documents/lhre-2026/BEVO/dashd/deploy/launch_kiosk.sh` |
| What CAN frames is cand seeing?   | `candump can0`                                                |
| Is `dashd` actually emitting JSON?| `curl --include --no-buffer --header "Connection: Upgrade" --header "Upgrade: websocket" --header "Sec-WebSocket-Version: 13" --header "Sec-WebSocket-Key: dGVzdA==" http://localhost:8001/` (or use `wscat -c ws://localhost:8001` if installed) |
