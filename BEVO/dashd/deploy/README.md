# BEVO dash deploy

What's in this directory:

| File                          | Where it lives on the Pi                            | Purpose                                                  |
|-------------------------------|------------------------------------------------------|----------------------------------------------------------|
| `bevo_dash_serve.service`     | `/etc/systemd/system/`                              | systemd unit; serves the React build over HTTP :8080    |
| `dash-kiosk.desktop`          | `~/.config/autostart/`                              | Auto-launches Chromium kiosk on graphical login          |
| `launch_kiosk.sh`             | runs in place                                       | Waits for the server, then `chromium --kiosk` at it      |
| `install.sh`                  | runs in place                                       | Copies the two config files above, enables the unit     |

## How the runtime fits together

On the Pi at boot:

1. `bevo_telemetry.service` (in `BEVO/nonhermetic/`) launches `cand` + `dashd` + `publishd` + `loggerd`. Already in place — nothing new for the dash.
2. `bevo_dash_serve.service` (this dir) runs `python3 -m http.server 8080` from the React build directory. Serves HTML/JS only.
3. After graphical login as `lhre`, `dash-kiosk.desktop` runs `launch_kiosk.sh`, which polls the static server and then opens Chromium kiosk-mode at `http://localhost:8080`.
4. The page's JS connects to `ws://localhost:8001` to receive `DashMessage` JSON from `dashd` at 30 Hz.

Two independent ports: HTTP on `8080` (static), WebSocket on `8001` (data). Same-machine, so CORS is not a concern.

## Install / update

On the Pi, after pulling new code and rebuilding (`cargo build --release` in `BEVO/`, `npm run build` in `BEVO/dashd/frontend/`):

```bash
bash BEVO/dashd/deploy/install.sh
```

Re-running `install.sh` is safe and idempotent. It will:

- Mark the kiosk launcher executable.
- Copy the systemd unit to `/etc/systemd/system/` and `daemon-reload`.
- Enable + start `bevo_dash_serve.service`.
- Drop the autostart `.desktop` file into `~/.config/autostart/`.
- Probe `http://localhost:8080` to confirm the server came up.

## First-run checklist

- [ ] Pi auto-logs into a graphical session as `lhre` (Raspberry Pi config → System → Boot → To Desktop, autologin).
- [ ] `chromium-browser` or `chromium` is installed (`sudo apt install chromium`).
- [ ] `python3` is installed (default on Pi OS).
- [ ] `BEVO/dashd/frontend/build/index.html` exists (from `npm run build`).
- [ ] `bevo_telemetry.service` is running so `dashd` is up on :8001.
- [ ] `install.sh` completed without warnings.
- [ ] After reboot, Chromium opens full-screen with the dash visible.

## Manual operations

- Restart the static server: `sudo systemctl restart bevo_dash_serve.service`
- View its logs: `journalctl -u bevo_dash_serve.service -f`
- Launch the kiosk manually (good for debugging): `BEVO/dashd/deploy/launch_kiosk.sh`
- Kill Chromium: `pkill -f chromium`

## If the kiosk launches but the dash is blank

Most likely `dashd` isn't running or the build directory is empty.

```bash
sudo systemctl status bevo_telemetry.service
sudo systemctl status bevo_dash_serve.service
ls /home/lhre/Documents/lhre-2026/BEVO/dashd/frontend/build/
curl http://localhost:8080/         # should return index.html
ss -tlnp | grep -E '8080|8001'      # both ports listening?
```
