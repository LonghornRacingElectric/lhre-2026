# Dash project handoff

This document is a self-contained briefing for whoever picks up the dash work
next (human or fresh Claude agent without prior session context). Skim the
TL;DR, then jump to the section you need.

---

## TL;DR

- **Project:** Real-time driver dashboard for the **UT Austin Longhorn Racing
  Electric (LHR)** FSAE EV car. Lives at `BEVO/dashd/` in this monorepo.
- **Branch:** `dash-mqtt`. Most recent commit: `dbac1088` at time of writing.
- **Stack:** A Rust daemon (`dashd`) reads decoded CAN data from another Rust
  daemon (`cand`) over a Unix socket, subscribes to an MQTT broker for off-car
  computed values, merges both, and serves a unified JSON stream over
  WebSocket to a React frontend. The frontend runs in Chromium kiosk mode on
  the BEVO board (a Pi-based custom board in the car).
- **Where things stand:** All software wiring is complete and pushed. Nothing
  has been verified on real hardware yet. The next session is a garage trip
  to the BEVO board to do first-light validation.
- **The user (Gray):** UT Austin student on the LHR team. Comfortable with
  software but **new to embedded / physical-hardware setup**. Prefers terse
  responses, short commit messages, and emphatically **does not want Claude
  added as a `Co-Authored-By:` on any commit**.

## Project context

### What BEVO is

BEVO = "Board Emitting Vehicle Outputs". It's a student-designed PCB with a
**Raspberry Pi Compute Module** on it, mounted in the car. The HDMI port on
BEVO drives the dashboard display the driver sees.

Username on the Pi is `lhre`. Repo on the Pi is checked out at
`/home/lhre/Documents/lhre-2026/`.

Existing on-Pi runtime:

- `bevo_telemetry.service` (systemd) at `BEVO/nonhermetic/bevo_telemetry.service`
  starts `start_telemetry.sh`, which: turns on the cellular modem via
  `cell.py on`, brings up `can0` at 1 Mbps, then runs `run_real_stack.sh` to
  launch the four daemons (`cand`, `dashd`, `publishd`, `loggerd`).
- Auto-stops after 1 h or when disk drops below 1 GB free.

### Daemons

| Daemon       | What it does                                                              | Listens on / talks to                                  |
|--------------|---------------------------------------------------------------------------|--------------------------------------------------------|
| `cand`       | Reads frames from `can0`, decodes per `can.json`, fills `OrionSensorData` | Unix sockets `/tmp/BEVO_cand.sock` (dashd/loggerd) and `/tmp/BEVO_cand_publishd.sock` (publishd); TCP `0.0.0.0:2000` for NMEA GPS |
| `dashd`      | Bridges cand IPC + MQTT broker → WebSocket JSON for the React frontend    | Reads `/tmp/BEVO_cand.sock`; serves WS on `:8001`; subscribes to `lhre/dash/#` on MQTT broker `18.191.225.118:1883` |
| `publishd`   | Forwards `OrionSensorData` to remote telemetry over MQTT                  | Not modified in this session                            |
| `loggerd`   | Logs CAN data to disk                                                     | Not modified in this session                            |

### React frontend

Lives at `BEVO/dashd/frontend/`. Plain Create React App + TypeScript. Three
screens:

- **`ScreenOne.tsx`** — the *driving screen*. Speed front and center, lap
  timing in a 2×2 grid (CURR / LAP DELTA / BEST / LAST), SOC + pack temp as
  vertical edge bars, big bidirectional power bar at the bottom, plus a
  bottom tray with TC + REGEN indicators, Energy Delta, Connectivity (5G),
  and System OK/FAULT.
- **`ScreenTwo.tsx`** — the *shutdown* screen. 4×4 grid of shutdown-circuit
  items, color-coded green/red/grey. Driven by `SHUTDOWN_NAMES` from
  `types/DashData.ts` plus 6 "awaiting firmware" placeholders.
- **`PitDiagnostic.tsx`** — the *pit / diagnostic* screen. Driver inputs
  (APPS, BPPS, brake pressures), powertrain temps, wheel speeds, fault list,
  and a bottom-tray with HV V / HV A / LV V / LV A / Odo.
- **`Settings.tsx`** — driver-name picker + a few settings.

Routing in `App.tsx`. There's a "demo mode" toggled by pressing `D` (see
`context/DashContext.tsx`). In demo mode the dash is fed by `useDemoData.ts`
which synthesizes plausible values; in live mode it reads from `useCarData.ts`
which talks to `ws://localhost:8001`.

### What's on this branch vs main

`dash-mqtt` is a feature branch ahead of `main`. Main was merged in during
this session (commit `3c8b423c`) so we're up-to-date with shared monorepo
changes (lots of firmware/telemetry/LVBMS work landed; none of it intersects
the dash work).

## State of the data wiring

### Field-by-field — driving screen

| Field            | Source (proto)                            | Live? | Notes                                                                  |
|------------------|-------------------------------------------|-------|------------------------------------------------------------------------|
| Speed            | `dynamics.gps_speed` × 1.15078 (knots→MPH) | ✅    | From GPS NMEA, not wheel speed.                                        |
| Power            | `pack.dc_bus_v * pack.dc_bus_current / 1000` (kW) | ✅ | Bar split 20% regen / 80% drive.                                       |
| SOC              | `pack.hv_soc`                              | ✅    |                                                                        |
| Pack temp (TEMP) | `thermal.cell_top_temp`                    | ✅    | Frontend can also use `cellTempMax` aggregate.                         |
| BB (brake bias)  | `controls.brake_bias`                      | ✅    | (Newly wired this session.)                                            |
| Lap delta        | MQTT `lhre/dash/lapDelta`                  | ⚠️    | Needs an MQTT publisher upstream — see `BEVO/dashd/MQTT_CONTRACT.md`.   |
| Energy delta     | MQTT `lhre/dash/energyDelta`               | ⚠️    | Same.                                                                  |
| Laps remaining   | MQTT `lhre/dash/lapsRemaining`             | ⚠️    | Same.                                                                  |
| Best/last/cur lap| Not in any source today                    | ❌    | Frontend optional fields. Future on-car timer or off-car MQTT.        |
| Lap delta rate   | Not in any source today                    | ❌    | Could be computed in dashd as `d(lapDelta)/dt`.                        |
| Laps rem (NRG)   | Not in any source today                    | ❌    | Off-car compute.                                                       |
| TC level / enabled | Not in any source today                  | ❌    | Steering-wheel rotary/switch — VCU would need a CAN message.           |
| Regen enabled    | Not in any source today                    | ❌    | Same.                                                                  |
| Shutdown circuit | `diagnostics_low.shutdown_legX` ×4 + `bmb_comm_error` + `imd_gnd_isolation_error` (+ 4 more after sync_assets) | ✅ | 10-element array; see "Shutdown wiring" below. |
| Connectivity (5G)| Not in any source today                    | ❌    | `cell.py` might be pollable for this — TBD.                            |

### Field-by-field — pit screen

All from the same `OrionSensorData` snapshot:

| Field              | Source                                                 |
|--------------------|--------------------------------------------------------|
| APPS               | `controls.apps1_travel`                                |
| BPPS               | `controls.bpps1_travel`                                |
| Brake pressure F   | `controls.brake_pressure_f`                            |
| Brake pressure R   | `(controls.brake_pressure_rall + brake_pressure_rbll)/2` (averaged — see comment in `extract_can_data`) |
| Motor T            | `thermal.motor_temp`                                   |
| Inverter T         | `thermal.inverter_temp`                                |
| Coolant T          | `thermal.coolant_temp`                                 |
| Cell T min/avg/max | Aggregated from `pack.cells_temps[]`                   |
| HV V               | `pack.hv_pack_v`                                       |
| HV A               | `pack.hv_c`                                            |
| LV V               | `pack.lv_batt_v`                                       |
| LV A               | `pack.lv_batt_c`                                       |
| Wheel speeds (4)   | `dynamics.flw_speed / frw_speed / blw_speed / brw_speed` |
| Odometer           | Always null — would need integrated wheel-speed-over-time. |

### Shutdown wiring (subtle)

Originally the frontend assumed a 16-element shutdown array. Reality is
messier:

- The `OrionSensorData` proto exposes **6** shutdown-circuit booleans today
  (`shutdown_leg1..4` + `bmb_comm_error` + `imd_gnd_isolation_error`).
- `can.json` ALSO decodes `shutdown_bspd_status`, `shutdown_emeter_status`,
  `temp_shutdown_1`, `temp_shutdown_2` from the CAN bus, but those bits were
  being **silently dropped** because the proto file didn't have fields for
  them. This session added the fields to the proto (`drivers/longhorn-lib/protobuf/can_packets.proto`
  + `BEVO/nonhermetic/assets/can_packets.proto`). Until someone runs
  `BEVO/nonhermetic/sync_assets.sh` on a Linux box that has bazel or protoc,
  the four new bits remain at default `false` because
  `BEVO/generated_mapping.rs` is checked-in code that codegen.py produces
  from the proto. **Important: cargo build does NOT regenerate
  generated_mapping.rs.** It only regenerates the proto-derived Rust structs.
- The other ~6 items from the original 16-item list (TSMS, MSD HVIL, BOTS,
  L-ESTOP, R-ESTOP, D-ESTOP, etc.) **are not on CAN at all** in the current
  firmware. They'd need a sense line + a CAN message added by the electrical
  team. ScreenTwo shows them as 6 placeholder cells with "Awaiting firmware"
  status.

So today's order in dashd's `shutdown[]` (post-sync_assets):

```
[0] LEG 1     (diagnostics_low.shutdown_leg1)
[1] LEG 2     (diagnostics_low.shutdown_leg2)
[2] LEG 3     (diagnostics_low.shutdown_leg3)
[3] LEG 4     (diagnostics_low.shutdown_leg4)
[4] BMS       (!diagnostics_low.bmb_comm_error)
[5] IMD       (!diagnostics_low.imd_gnd_isolation_error)
[6] BSPD      (diagnostics_low.shutdown_bspd_status)        ← need sync_assets
[7] E-METER   (diagnostics_low.shutdown_emeter_status)      ← need sync_assets
[8] DUI TEMP 1 (!diagnostics_low.temp_shutdown_1)           ← need sync_assets
[9] DUI TEMP 2 (!diagnostics_low.temp_shutdown_2)           ← need sync_assets
```

Convention everywhere: `true = OK`, `false = FAULT`. Fields with `*_error` /
`temp_shutdown_*` semantics are inverted in `extract_can_data` so the
shutdown convention holds throughout.

`SHUTDOWN_NAMES` (in `BEVO/dashd/frontend/src/types/DashData.ts`) is the
authoritative ordering. ScreenTwo iterates a `CELLS` superset (10 from
SHUTDOWN_NAMES + 6 firmware-placeholder entries) to fill the 4×4 grid.

### Stale-data handling

- **MQTT side:** Each field tracked in `MqttState` with a per-field `Instant`;
  fields go to null after `MQTT_STALE_TIMEOUT = 5 s` of silence.
- **CAN side (new this session):** `DashState.last_can_update` tracked
  globally; if `> CAN_STALE_TIMEOUT = 3 s` since last successful IPC decode,
  the WS sender emits `CanData::default()` (all null) instead of frozen
  last-known values.

So a dash showing all `--` for CAN fields means cand has been silent for
≥3 s, not that the screen is broken.

## What was done in this session

Recent commits, newest first:

| Commit     | Summary                                                                 |
|------------|-------------------------------------------------------------------------|
| `dbac1088` | `dashd/deploy: add deploy.sh + expanded Pi runbook`                     |
| `f5e9438f` | `dashd: null CanData after 3 s of cand silence`                         |
| `73efb12d` | `dashd: add kiosk + static-serve deploy artifacts for the Pi`           |
| `a51cf3d8` | `ScreenTwo: fill 16-cell grid with 6 awaiting-firmware placeholders`    |
| `e448e471` | `ScreenTwo: drive layout from SHUTDOWN_NAMES so labels match dashd order` |
| `f48e5894` | `proto: add BSPD/E-meter/DUI thermal shutdown bits + forward in dashd`  |
| `79ef69b1` | `dashd: forward HV/LV/brake/thermal/wheel data + BMS/IMD in shutdown`   |
| `7c9dcaca` | `gitignore: add .claude/`                                               |
| `3c8b423c` | `Merge branch 'main' into dash-mqtt`                                    |
| `7c68f9af` | `Dash + pit screens: layout polish, demo data for diag, pinned briefing values` |

Highlights of what those commits actually do:

- **Field wiring (`79ef69b1`).** Extended `dashd`'s `CanData` struct + the
  TS `CanData` interface to carry HV V/A, LV V/A, brake bias, APPS, BPPS,
  brake pressures, motor/inv/coolant temps, cell aggregates, wheel speeds,
  and a 6-element shutdown array. All sourced from existing protobuf fields,
  none of which were being forwarded before.
- **Proto extension (`f48e5894`).** Added four bool fields to
  `DiagnosticsLow` — `shutdown_bspd_status`, `shutdown_emeter_status`,
  `temp_shutdown_1`, `temp_shutdown_2`. The CAN frames carrying these bits
  were already being decoded by `can.json`, but the bits had nowhere to land
  because the proto lacked fields. Now they do. Shutdown array grew to 10.
  ⚠️ **`generated_mapping.rs` was NOT regenerated** because we were on
  Windows where bazel/protoc isn't available. See "Things to verify on the
  Pi" below.
- **ScreenTwo refactor (`e448e471`, `a51cf3d8`).** ScreenTwo previously had
  its own 16-item label list that didn't match `SHUTDOWN_NAMES` indices.
  After Commit A's shutdown wiring, the labels would have been wrong (e.g.,
  `shutdown[3]` = LEG 4 but ScreenTwo showed it under "BMS"). Fixed by
  driving ScreenTwo's layout off `SHUTDOWN_NAMES` plus 6 firmware-pending
  placeholders.
- **Kiosk + serve plumbing (`73efb12d`).** New `BEVO/dashd/deploy/`
  directory with: a systemd unit (`bevo_dash_serve.service`) running
  `python3 -m http.server 8080` bound to `127.0.0.1` from
  `BEVO/dashd/frontend/build/`; a Chromium kiosk launcher
  (`launch_kiosk.sh`); an autostart `.desktop` entry; an idempotent
  `install.sh`; a focused `.gitattributes` locking these files to LF.
- **CAN stale detect (`f5e9438f`).** Added `last_can_update` to `DashState`,
  bumped by `ipc_reader_loop` on every successful decode; checked by
  `ws_server_loop` against `CAN_STALE_TIMEOUT = 3 s` to decide whether to
  send the cached `CanData` or a default-init one.
- **Deploy script + runbook (`dbac1088`).** `deploy.sh` runs `git pull` →
  `cargo build --release` → `npm run build` → `systemctl restart`.
  `README.md` in `BEVO/dashd/deploy/` walks through first-time Pi setup,
  the update workflow, first-run checklist, and diagnostics for common
  failure modes.

The session also did demo-data work earlier (commits before this list).
Pinned values in `useDemoData.ts` for the design briefing: HV V 449.3,
HV A 10, LV V 25, LV A 9.3, Odo 101.3. These are static intentionally for
the briefing photo — they're easy to revert if you want movement back.

## Open questions / risks (read these)

1. **`generated_mapping.rs` is out of date relative to the proto.** The four
   new shutdown bits added in `f48e5894` won't actually populate from CAN
   until someone runs `BEVO/nonhermetic/sync_assets.sh` on a Linux box that
   has `bazel` or `protoc`. The script regenerates `BEVO/sensor_data.desc`
   and `BEVO/generated_mapping.rs`. **`cargo build --release` alone does NOT
   do this** — `BEVO/build.rs` only handles `prost_build` of the proto-Rust
   structs. cand will keep silently dropping the four new bits until regen
   runs. The fix is one command on the Pi. See the on-Pi setup section
   below.

2. **Bazel / hermetic build path.** The canonical proto is at
   `drivers/longhorn-lib/protobuf/can_packets.proto`. The vendored copy used
   by nonhermetic builds is at `BEVO/nonhermetic/assets/can_packets.proto`.
   Both were updated identically. The hermetic Bazel build pipeline almost
   certainly uses the canonical one, but I haven't verified it.

3. **`telemetry/.../viewer_tool/protobuf/orion.proto`** is yet another copy of
   the proto, used by the off-car telemetry viewer. It was NOT updated. proto3
   forward-compat means existing parsers won't crash on unknown fields, but
   the viewer won't see the four new shutdown bits if it ever cares.

4. **Compilation never verified on Linux.** All the Rust changes are
   mechanical, but the working machine is Windows where `cand`/`dashd` won't
   build (they use `socketcan` + Unix sockets). First `cargo build` on the
   Pi might surface a typo / wrong field name. If it does, the fix is
   probably obvious — the proto field names in `extract_can_data` should
   match `orion.proto` exactly.

5. **No tests.** There are no unit tests on `dashd`. End-to-end validation
   is just "run the mock stack and look at the dash."

6. **MQTT publisher is upstream and probably not running.** Off-car compute
   (lap delta, energy delta, laps remaining) needs SOMEONE publishing to
   `lhre/dash/#` on `18.191.225.118:1883`. If nobody is, those three fields
   stay null and the top bar / energy delta read `--`. Not our problem to
   fix but worth being aware of.

7. **`signalStrength` is null.** The 5G modem is controlled via
   `BEVO/cell.py` (read it for context); polling its signal status to expose
   over the dash is a future-work item, not addressed this session.

## Going to the garage — what should happen on the Pi

The full procedure is in `BEVO/dashd/deploy/README.md`. The fast path:

1. **SSH or physical-keyboard onto the Pi.** Username `lhre`. If you don't
   know the IP, ask the user — they may need to look it up via the network's
   DHCP table or run `hostname -I` from a directly-connected keyboard.
2. **Pull and rebuild.** If first-time:
   ```bash
   # First-time setup section in deploy/README.md has the full prereq list.
   cd ~/Documents/lhre-2026
   git pull
   bash BEVO/nonhermetic/sync_assets.sh   # only needed once after proto changes
   cd BEVO && cargo build --release
   cd dashd/frontend && npm install && npm run build
   cd ../..
   bash dashd/deploy/install.sh
   sudo cp nonhermetic/bevo_telemetry.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now bevo_telemetry.service
   ```
   If subsequent run:
   ```bash
   bash ~/Documents/lhre-2026/BEVO/dashd/deploy/deploy.sh
   ```
3. **Reboot to verify the kiosk launches clean.**
4. **Validate live data.** If the dash shows `--` everywhere after 3 s,
   either `cand` isn't running, the CAN bus isn't up, or the protobuf isn't
   getting decoded. Use the diagnostics section of the deploy README.

## User working preferences

These are critical — the new agent won't have them via memory.

- **NEVER add `Co-Authored-By: Claude` (or any Claude co-author trailer) to
  any commit.** The user explicitly requested this.
- **Brief commit messages.** One short subject line. No body, no bullets,
  no trailers. Like `dashd: foo` not `feat(dashd): add foo with comprehensive
  bar handling`.
- **Terse responses.** The user prefers short, direct communication. No
  preamble, no recap before action, no "happy to help" filler. Match the
  pattern in `BEVO/dashd/deploy/README.md` and the runbook content — facts,
  tables, code blocks, short paragraphs.
- **Don't add features that weren't asked for.** Don't refactor adjacent
  code. Don't add defensive validation for impossible cases. Three similar
  lines beat a premature abstraction. Bug fixes don't need surrounding
  cleanup.
- **Pick reasonable defaults when blocked.** If the user said "make HV
  match FSAE limits" and didn't give a number, look it up, pick something
  defensible, document the choice in a comment, and move on. Don't ping
  back asking for clarification on every detail.
- **Memory exists.** Auto-memory is at
  `C:\Users\Gray\.claude\projects\C--Users-Gray-WebstormProjects-lhre-2026\memory\`.
  `MEMORY.md` is the index. `feedback_commits.md` already captures the
  no-coauthor + brief-messages preference. New laptops won't have this
  populated — *this document* is the substitute briefing for the new agent.

## File map

Code by purpose:

| Concern                  | Files                                                         |
|--------------------------|---------------------------------------------------------------|
| dashd daemon entry       | `BEVO/dashd/main.rs`                                          |
| dashd MQTT contract      | `BEVO/dashd/MQTT_CONTRACT.md`                                  |
| cand daemon entry        | `BEVO/cand/main.rs`                                            |
| Rust build script        | `BEVO/build.rs` (proto codegen via `prost_build`)              |
| Proto schema (canonical) | `drivers/longhorn-lib/protobuf/can_packets.proto`              |
| Proto schema (vendored)  | `BEVO/nonhermetic/assets/can_packets.proto`                    |
| CAN-id → proto field map | `BEVO/nonhermetic/assets/can.json`                             |
| Generated mapping (Rust) | `BEVO/generated_mapping.rs` (regenerate via `sync_assets.sh`)  |
| Frontend entry           | `BEVO/dashd/frontend/src/App.tsx`                              |
| Driving screen           | `BEVO/dashd/frontend/src/screens/ScreenOne.tsx`                |
| Shutdown screen          | `BEVO/dashd/frontend/src/screens/ScreenTwo.tsx`                |
| Pit / diag screen        | `BEVO/dashd/frontend/src/screens/PitDiagnostic.tsx`            |
| Settings                 | `BEVO/dashd/frontend/src/screens/Settings.tsx`                 |
| TS types                 | `BEVO/dashd/frontend/src/types/DashData.ts`                    |
| Demo data generator      | `BEVO/dashd/frontend/src/hooks/useDemoData.ts`                 |
| Live WS client           | `BEVO/dashd/frontend/src/hooks/useCarData.ts`                  |
| Dash context (D toggles) | `BEVO/dashd/frontend/src/context/DashContext.tsx`              |
| Connectivity widget      | `BEVO/dashd/frontend/src/components/ConnectivityIndicator.tsx` |
| Deploy artifacts         | `BEVO/dashd/deploy/` (this directory)                          |
| On-Pi runbook            | `BEVO/dashd/deploy/README.md`                                  |
| Update script            | `BEVO/dashd/deploy/deploy.sh`                                  |
| Telemetry service        | `BEVO/nonhermetic/bevo_telemetry.service`                      |
| Stack launcher           | `BEVO/nonhermetic/run_real_stack.sh`                           |
| Asset sync               | `BEVO/nonhermetic/sync_assets.sh`                              |

## Quick command reference

```bash
# Switch to dash branch (if not already)
git checkout dash-mqtt

# Pull / rebuild / restart on the Pi
bash ~/Documents/lhre-2026/BEVO/dashd/deploy/deploy.sh

# After proto changes — regenerate mapping (Linux only)
bash ~/Documents/lhre-2026/BEVO/nonhermetic/sync_assets.sh

# Mock stack on a dev box (Linux/WSL)
bash ~/Documents/lhre-2026/BEVO/nonhermetic/run_full_mock_stack.sh

# Frontend dev / iteration (any OS)
cd BEVO/dashd/frontend && npm start

# Frontend production build
cd BEVO/dashd/frontend && npm run build

# Status of the on-car stack
sudo systemctl status bevo_telemetry.service
sudo systemctl status bevo_dash_serve.service

# Tail logs
journalctl -u bevo_telemetry.service -f
journalctl -u bevo_dash_serve.service -f

# What's on the CAN bus right now
candump can0

# Confirm dashd is emitting at :8001 (needs wscat installed; or watch
# Chromium devtools network tab in kiosk)
wscat -c ws://localhost:8001

# Confirm the static server is serving
curl -s http://localhost:8080/ | head

# Manually relaunch the kiosk after killing it
pkill chromium
~/Documents/lhre-2026/BEVO/dashd/deploy/launch_kiosk.sh
```

## Where to read more

- `BEVO/dashd/deploy/README.md` — full Pi setup + update workflow + diagnostics
- `BEVO/dashd/MQTT_CONTRACT.md` — exactly what off-car publishers should send
- `BEVO/nonhermetic/README.md` — nonhermetic build/run conventions
- `BEVO/README.md` — top-level BEVO description (short)
- `BEVO/dashd/main.rs` — read the top half: constants, structs, `extract_can_data`
- `BEVO/dashd/frontend/src/screens/ScreenOne.tsx` — most of the visual design
  decisions are encoded in the JSX + style props here

## Final note for the next agent

The user is doing this work in a tight window (FSAE car prep), is new to
hardware, and explicitly asked for a handoff doc so a fresh agent in the
garage can pick up cleanly. They are sharp on software and will course-
correct fast if you guess wrong. When in doubt: read the relevant file,
state your read of the situation, then act — don't ask three clarifying
questions in a row. Match the brevity and directness of the commits and
existing docs.
