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

### Endurance pacing signals

`targetPower` and `lapTrigger` are published by the **Dash** tab on the
trackside-live page (`telemtry/analysis/database/viewer_tool`) over the broker's
websockets listener (port `8080`). They drive on-dash endurance pacing — the
dash integrates CAN power locally and compares it to the budget, so only these
two control values come off-car:

- **`targetPower`** — the live power budget (kW) the strategist dials in. The
  dash integrates real power against `targetPower * elapsed` per lap; the
  top energy bar runs green while under budget and red while over. It's a
  set-point, so the sender **republishes it ~1 Hz** to beat the 5 s staleness
  null (below).
- **`lapTrigger`** — a monotonically increasing lap counter. On each **increase**
  the dash pops a full-screen lap card (lap time + energy used that lap) and
  resets the per-lap energy integrator, so pacing error can't accumulate across
  laps. The dash keys off the rising edge, not the absolute value, so staleness
  nulls between laps are harmless.

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
