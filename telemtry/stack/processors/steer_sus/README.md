# steer_sus processor

Consumes `sensor_data`, runs front/rear suspension FMUs from `kin_backend/`, and publishes
hardpoint geometry to Kafka topic `steer_sus`.

## Pipeline

1. Decode protobuf packet (`Angelique`/`Orion`/`Nightwatch`) from `sensor_data`.
2. Extract suspension inputs (`fl/fr/bl/br_sus_pot_v`, with `sus1_v/sus2_v` fallback).
3. Convert sensor inputs to wheel travel via `kin_backend/core/lookup.py` (`maps.npz`).
4. Drive FMUs:
   - `kin_backend/fmus/FrKnCFMI.fmu`
   - `kin_backend/fmus/RrKnCFMI.fmu`
5. Publish:
   - `state` (z/heave/roll/length inputs)
   - `geometry.front.*` and `geometry.rear.*`
   - flattened front and rear signals from `signals.txt` (including rear hardpoints)

## Real-time behavior

- Uses latest-point processing: each poll batch is reduced to the newest record.
- With `STEER_SUS_SKIP_CATCHUP=1` (default), offsets are advanced to end after processing,
  so the processor stays near real-time instead of replaying backlog.
- For Orion-style inputs, raw suspension values are normalized to the lookup-map length
  domain (supports direct lengths, voltage-like inputs, and mm-like ride-height values).

## Environment variables

- `KAFKA_BOOTSTRAP_SERVERS` (default: `kafka:9092`)
- `KAFKA_INPUT_TOPIC` (default: `sensor_data`)
- `KAFKA_OUTPUT_TOPIC` (default: `steer_sus`)
- `KAFKA_GROUP_ID` (default: `steer-sus-group`)
- `KAFKA_DEFAULT_CAR` (default: `Orion`)
- `KAFKA_MAX_POLL_RECORDS` (default: `200`)
- `KAFKA_POLL_TIMEOUT_MS` (default: `1000`)
- `STEER_SUS_SKIP_CATCHUP` (default: `1`)
- `STEER_SUS_LOG_INTERVAL_S` (default: `1.5`)
- `STEER_SUS_OUTPUT_FRAME` (default: `viewer`; set `raw` to disable frame remap)
- `STEER_SUS_ROTATE_X_DEG` (default: `0`)
- `STEER_SUS_ROTATE_Y_DEG` (default: `0`)
- `STEER_SUS_ROTATE_Z_DEG` (default: `0`)
- `LOGLEVEL` (default: `INFO`)

## FMU runtime note

- `kin_backend/fmus/*.fmu` Linux binaries require newer glibc (`GLIBC_2.38+`).
- The processor Docker image uses `ubuntu:24.04` to satisfy this runtime requirement.
