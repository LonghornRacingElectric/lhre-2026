# Dash MQTT Contract

The on-car dash (`dashd`) subscribes to the following MQTT topics to display
values computed off-car. Any system that publishes to these topics will have
its data shown on the driver's dashboard in real time.

## Broker

- Host: `18.191.225.118` (AWS, same broker as publishd)
- Port: `1883`
- No authentication required

## Topics

| Topic                       | Payload        | Unit              | Example  |
|-----------------------------|----------------|-------------------|----------|
| `lhre/dash/lapDelta`       | JSON number    | seconds           | `-0.45`  |
| `lhre/dash/energyDelta`    | JSON number    | Wh                | `3.2`    |
| `lhre/dash/lapsRemaining`  | JSON number    | laps (fractional) | `15.3`   |
| `lhre/dash/targetPower`    | JSON number    | kW                | `32`     |
| `lhre/dash/lapTrigger`     | JSON number    | monotonic counter | `7`      |
| `lhre/dash/layout`         | JSON object    | lap-card layout   | `{"version":1,"widgets":[…]}` |
| `lhre/dash/sfGate`         | JSON `[f64;4]` | `[lat1,lon1,lat2,lon2]` | `[30.39,-97.72,30.39,-97.73]` |

### Endurance pacing signals

`targetPower` and `lapTrigger` are published by the **Dash** tab on the
trackside-live page (`telemtry/analysis/database/viewer_tool`) over the broker's
websockets listener (port `8080`). They drive on-dash endurance pacing — the
dash integrates CAN power locally and compares it to the budget, so only these
two control values come off-car:

- **`targetPower`** — the live power budget (kW) the strategist dials in. The
  dash integrates real power against `targetPower * elapsed` per lap; the
  top energy bar runs green while under budget and red while over. It's a
  set-point, so the sender **republishes it ~1 Hz**. Unlike the other fields it
  is **held last-known on the dash across a dropout** (not nulled): instead the
  dash emits `targetPowerStale: true` to the frontend, which dims the bar and
  shows a "STALE" badge — a held budget beats a blank one for the driver.
- **`lapTrigger`** — a monotonically increasing lap counter. On each **increase**
  the dash pops a full-screen lap card (lap time + energy used that lap) and
  resets the per-lap energy integrator, so pacing error can't accumulate across
  laps. The dash keys off the rising edge, not the absolute value. This is now a
  **fallback/override**: when an `sfGate` is loaded the car counts its own laps.
**Reverse channel (dash → trackside).** dashd also *publishes* so the strategist
can confirm the uplink and mirror the driver's screen:

- **`lhre/dash/state`** — JSON snapshot at ~2 Hz of what the driver sees (speed,
  power, soc, temperature, lap count, and the on-car pacing: `lapEnergyWh`,
  `budgetDeltaWh`, `lapNumber`, last-lap time/energy). The Dash tab renders this
  as a live mirror; if it goes silent for >3 s the panel flags the uplink down.
- **`lhre/dash/ack/{targetPower,lapTrigger,sfGate}`** — retained echoes published
  the moment dashd ingests each control, so trackside sees "the car heard 32 kW
  2 s ago" rather than just "I sent it." Energy integration is authoritative
  on-car (survives a chromium reload), so these are the same numbers, not a
  re-derivation.

- **`sfGate`** — the start/finish line as `[lat1, lon1, lat2, lon2]`, published
  **retained, QoS 1** from the Dash tab's "Push S/F to car" button (sourced from
  the Track Builder gate). Once loaded, dashd watches `dynamics.gps` and bumps
  the lap counter when the car's path crosses the line — so **the per-lap reset
  no longer depends on the link at all.** The gate is cached to disk
  (`DASHD_SFGATE_PATH`, default `/tmp/BEVO_dash_sfgate.json`) so it also survives
  a reboot if the broker drops its retained copy.
- **`layout`** — the website-authored **lap-card layout** (a `LapCardLayout` JSON
  object), published **retained, QoS 1** by the Dash tab's lap-screen designer
  (sent once, used until replaced). dashd holds it, caches it to disk
  (`DASHD_LAYOUT_PATH`, default `/tmp/BEVO_dash_layout.json`), forwards it to the
  frontend as `DashMessage.layout`, and echoes `lhre/dash/ack/layout`. The
  frontend validates it and renders text/value/delta/bar/gauge widgets on the lap
  card; a missing or malformed layout falls back to the built-in card so the
  driver screen never blanks. Schema/renderer: `dashd/frontend/src/dashLayout.ts`
  + `LapCardRenderer.tsx` (shared with the website editor; that copy is the
  source of truth).

## Payload format

Each payload is a **bare JSON number** — no wrapping object, no quotes. Examples:

```
-0.45
3.2
15.3
```

Invalid payloads (non-numeric, empty, JSON objects) are silently ignored.

## QoS

Use QoS 0 (AtMostOnce). This is real-time display data; retransmission of
stale values is worse than dropping them.

## Staleness

If dashd does not receive a message on a topic for **5 seconds**, that field
is sent to the frontend as `null` (displayed as "--" on the dash). Publishing
resumes normal display immediately.

## Publish rate

Recommended: 1–10 Hz. The dash updates at 30 Hz but only shows the latest
value, so publishing faster than 30 Hz is wasteful.

## Testing

```bash
# Publish a lap delta from any machine with mosquitto-clients:
mosquitto_pub -h 18.191.225.118 -t "lhre/dash/lapDelta" -m "-0.45"

# Publish all three:
mosquitto_pub -h 18.191.225.118 -t "lhre/dash/lapDelta" -m "-0.45"
mosquitto_pub -h 18.191.225.118 -t "lhre/dash/energyDelta" -m "3.2"
mosquitto_pub -h 18.191.225.118 -t "lhre/dash/lapsRemaining" -m "15.3"

# Endurance pacing: set a 32 kW budget, then trigger a lap card:
mosquitto_pub -h 18.191.225.118 -t "lhre/dash/targetPower" -m "32"
mosquitto_pub -h 18.191.225.118 -t "lhre/dash/lapTrigger" -m "1"
```
