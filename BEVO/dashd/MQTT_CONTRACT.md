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
```
