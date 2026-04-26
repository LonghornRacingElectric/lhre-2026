# Orion CAN Board Last-Seen Pipeline

This document describes the full **board last-seen** telemetry pipeline for Orion and how it flows from BEVO CAN ingest into server storage and Grafana.

## Scope

Tracked boards:

- CSM
- DUI
- HVC
- Inverter
- PDU
- TSM
- USM
- VCU

The value published for each board is:

- `-1`: board has never been seen since startup
- `>= 0`: seconds since the last observed CAN frame from that board

## Stack walkthrough

1. **BEVO `cand`**
   - Reads CAN frames from SocketCAN (`can0`) or mock UDP.
   - Parses `BEVO/nonhermetic/assets/can.json`.
   - Uses CAN packet `from` metadata to map packet IDs (including quantity-expanded ranges) to source boards.
   - Maintains in-memory last-seen timers and resets board timer to `0` when a frame arrives.
   - Increments all seen-board timers each publish tick.
   - Writes values into protobuf `OrionSensorData.board_status`.

2. **BEVO `publishd`**
   - Forwards serialized `OrionSensorData` protobuf over MQTT topic `orion`.

3. **Telemetry ingest (`telemtry/stack/ingest/mqtt_handler.py`)**
   - Ingests `orion` protobuf via `can_packets_pb2`.
   - Persists structured rows via `QueryBuilder`.
   - Includes `board_status.*` in Orion CSV logging headers.
   - Forwards payload to Kafka bridge gRPC.

4. **Kafka bridge (`telemtry/stack/kafka/cmd/bridge`)**
   - Decodes Orion protobuf with `sensor.proto`.
   - Flattens to JSON for `grafana_data_orion`.
   - Exposes board status fields (`*_last_seen_s`) for real-time dashboards.

5. **Database/ORM surfaces**
   - `board_status` table keyed by `packet_id`.
   - SQLAlchemy model + Prisma model + generated Orion helper models include `board_status`.

6. **Grafana**
   - New dashboard `orion/real-time/board_status.json`.
   - One gauge per board using `grafana_data_orion` live topic.

## Protobuf contract

Added message:

```proto
message BoardStatus {
  float csm_last_seen_s = 1;
  float dui_last_seen_s = 2;
  float hvc_last_seen_s = 3;
  float inverter_last_seen_s = 4;
  float pdu_last_seen_s = 5;
  float tsm_last_seen_s = 6;
  float usm_last_seen_s = 7;
  float vcu_last_seen_s = 8;
}
```

Added root field:

```proto
BoardStatus board_status = 9;
```

Updated in:

- `drivers/longhorn-lib/protobuf/can_packets.proto` (canonical)
- `BEVO/nonhermetic/assets/can_packets.proto`
- `telemtry/analysis/database/viewer_tool/protobuf/orion.proto`
- `telemtry/stack/kafka/proto/sensor/sensor.proto`
- `telemtry/stack/ingest/protobuf/can_packets_pb2.py`

## Generation/sync notes

- `drivers/longhorn-lib/scripts/generate_can_proto.py` now emits `BoardStatus` and `board_status` automatically, so `update_can_proto.py` preserves these fields.
- `BEVO/nonhermetic/sync_assets.sh` and `setup_local_env.sh` now sync `can_packets.proto` from `drivers/longhorn-lib/protobuf/can_packets.proto` when monorepo sources are available.

## Runtime bring-up checklist

1. Rebuild BEVO binaries (cand/publishd) using nonhermetic or Bazel flow.
2. Rebuild/restart ingest and kafka bridge containers/services.
3. If using an existing Orion DB volume, add `board_status` table (or reset/re-init schema).
4. Start stack and verify:
   - MQTT Orion payload contains `board_status`
   - `grafana_data_orion` has `*_last_seen_s`
   - Orion DB has rows in `board_status`
   - Orion CSV log contains `board_status.*`
   - Grafana dashboard `Board Status - Orion (Real-time)` updates per board

## Expected behavior

- Board values oscillate near `0` while frames are arriving.
- Values rise when a board stops transmitting.
- Boards never seen since process start stay at `-1`.
