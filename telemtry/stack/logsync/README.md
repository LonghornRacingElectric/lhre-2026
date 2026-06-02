# logsync — on-demand log retrieval from the BEVO Pi

Pulls `BEVO/loggerd/logs` CSV files from the car's Raspberry Pi to the server
within a requested **time range**, then serves them for download through the
viewer tool. Built to run while the car is parked between sessions without
hogging the cellular uplink during a run.

## What it does

- **Time-range selection.** Loggerd writes one file per session,
  `orion_<startMs>.csv`, appending until the session ends. A file's window is
  `[startMs, mtime]`; a job pulls every file whose window overlaps your request.
- **rsync over SSH over Tailscale.** `rsync --partial --append-verify` makes
  every transfer resumable: killing and re-running continues from the byte it
  left off and correctly extends the still-growing active log.
- **Pause when the car moves.** The worker watches the freshest
  `dynamics` row in the Orion DB; if wheel/GPS speed is above threshold and the
  sample is fresh, it kills rsync and waits. When telemetry is stale or absent
  there is no live stream to protect, so it transfers freely.
- **Survives restarts.** Job state is persisted to SQLite; interrupted jobs are
  re-queued and resumed on boot.
- **One transfer at a time** to keep bandwidth predictable.

### Known limitation
Selection is **file-granular**. Because a logging session can run for many
hours into a single multi-GB file, requesting a narrow window still pulls any
file whose session overlaps it. Row-level CSV slicing after transfer is a
possible future enhancement.

## Endpoints (proxied by the viewer at `/api/logsync/*`)

| Method | Path | Purpose |
| ------ | ---- | ------- |
| GET  | `/logs?from_ms&to_ms` | Dry-run: files + total bytes a job would pull |
| POST | `/jobs` | Create a job `{from_ms, to_ms, bwlimit_kbps?}` |
| GET  | `/jobs` / `/jobs/{id}` | List / get jobs |
| POST | `/jobs/{id}/pause\|resume\|cancel` | Control a job |
| GET  | `/jobs/{id}/files/{name}` | Download one CSV |
| GET  | `/jobs/{id}/archive` | Stream a ZIP of completed files |
| GET  | `/events` | SSE stream of job progress |
| GET  | `/motion` | Current motion reading (debug) |

## Deploy

1. **Provision the SSH key** (one time, on the server):
   ```bash
   ssh-keygen -t ed25519 -f telemtry/stack/logsync/keys/id_logsync -N ""
   ssh-copy-id -i telemtry/stack/logsync/keys/id_logsync.pub lhre@100.64.195.68
   # verify:
   ssh -i telemtry/stack/logsync/keys/id_logsync lhre@100.64.195.68 'echo ok'
   ```
   The key dir is git-ignored. The Pi must be reachable on the server's
   Tailscale (`tailscale status` should list `raspberrypi`).

2. **Confirm `rsync` is on the Pi** (it is on the current image: rsync 3.4.1).

3. **Bring it up** (db from the ingest stack must already be running so
   `localhost:5432` resolves):
   ```bash
   cd telemtry/stack/logsync
   docker compose up -d --build
   curl localhost:8090/health
   ```

4. **Point the viewer at it.** Set in the viewer's environment:
   ```
   LOGSYNC_URL=http://localhost:8090
   ```
   (or the server's address if the viewer runs elsewhere). The viewer exposes
   the UI at `/log-sync` and a tile on the splash page.

### Tuning (env)
See `.env.example`. Key knobs: `LOGSYNC_SPEED_THRESHOLD_MPS`,
`LOGSYNC_MOTION_STALENESS_MS`, `LOGSYNC_DEFAULT_BWLIMIT_KBPS`,
`BEVO_SSH_TARGET`, `BEVO_LOG_DIR`.

### Storage
The staging volume (`logsync_staging`) holds the copied CSVs — the current log
set is ~40 GB, individual files up to ~4 GB. Back it with roomy disk. The
worker refuses a job that wouldn't fit in free space.

## Networking note (host mode)

The container uses `network_mode: host` so it can reach **both** the Pi over the
host's Tailscale interface and the `db` container on `localhost:5432`. If host
networking isn't acceptable, run a Tailscale sidecar instead: add a `tailscale`
service (userspace or `/dev/net/tun` + `NET_ADMIN`) with a `TS_AUTHKEY`, attach
`logsync` to the `telemetry_network` for DB access, and set
`LOGSYNC_PG_HOST=db`. The transfer/motion logic is unchanged.
