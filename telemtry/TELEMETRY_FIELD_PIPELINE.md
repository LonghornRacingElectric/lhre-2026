# Telemetry field pipeline & how fields silently become 0/NULL

This document explains how a single signal travels from the car to a Grafana
panel, and the three failure modes that make a value silently read **0 / NULL**
even though the data is present somewhere in the stack. It exists because the
cell min/max voltage hit all three at once (see "Worked example" below).

## The pipeline

```
CAN signal (BEVO/nonhermetic/assets/can.json)
  -> can_packets.proto  (OrionSensorData = the on-wire source of truth)
  -> cand fills the proto, publishd -> AWS MQTT -> server ingest
       |
       |-- (A) Postgres: ingest decodes with can_packets_pb2.OrionSensorData,
       |        MessageToDict, then copies ONLY keys whose names match a model
       |        column (analysis/sql_utils/models.py, *_db_init.sql).
       |        Read by the "retrospect" Grafana dashboards (postgres datasource).
       |
       |-- (B) Realtime: ingest -> gRPC -> kafka-bridge orionToMap()
                (stack/kafka/cmd/bridge/main.go) builds a JSON map -> topic
                grafana_data_orion -> field_enricher (adds derived fields) ->
                grafana_data_orion_derived. Read by the "real-time" dashboards
                (kafka datasource).
```

Key point: **the proto is the only source of truth.** Every other layer
(Go bridge map, SQL columns, dashboard field/column names) is a hand-maintained
copy keyed by *name*. When the proto changes and a copy doesn't, the value
silently disappears — no error, just 0/NULL.

## The three failure modes

### 1. Orphaned sink field (name no longer produced)
A DB column / proto field / dashboard reference uses a name that **no live proto
field produces**. Name-based mapping finds no match and writes the column's
default (0/NULL).

- Example: `diagnostics_low.cell_min_v` (still in `template.proto`,
  `sensor.proto`, the generic `DiagnosticsLow` SQL model, `nightwatch_db_init.sql`).
  The current `OrionSensorData.DiagnosticsLow` has no such field — the BMS value
  moved to `thermal.min_cell_voltage`. So `cell_min_v` is an orphan: never written.
- Tell-tale: field numbering generations diverge (car Thermal uses 1–34;
  legacy server protos use a 1000+ block).

### 2. Dropped field in the bridge map (decoded but never copied)
`orionToMap()` must **explicitly** copy each proto field into the JSON map.
Anything not listed is decoded into the Go struct and then silently discarded —
so it never reaches Kafka or Grafana.

- Example: `thermal.min_cell_voltage` / `max_cell_voltage` existed in
  `sensor.OrionThermal` and were parsed correctly, but `orionToMap` only copied
  the module temps, not these two. (Fixed: they are now emitted.)

### 3. Zero-padded array aggregates
`cells_v` / `cells_temps` are fixed-length repeated floats. Cells that do not
report stay at the protobuf default `0.0`. Any naive `MIN`/`AVG` over the array
collapses (min -> 0, avg dragged low); `MAX` happens to survive, which is why
"min broken, max fine" is the classic symptom.

- Consumers must treat `0` as "no data" and exclude it:
  - Go bridge: `statsFromFloat32SliceNonZero` (skips zeros).
  - SQL: `MIN(value) FILTER (WHERE value > 0)`.
- The real root cause is upstream (cand emits a zero-padded `cells_v`); protobuf
  repeated scalars can't carry NULL, so filtering at consumption is the
  pragmatic fix. If the per-cell count becomes reliable, prefer the authoritative
  scalar `thermal.min_cell_voltage` over array-derived stats.

## Worked example: cell min voltage read 0 everywhere

- Realtime `min_cell_v` = `min(cells_v)` = 0 (mode 3).
- Retrospect `MIN(UNNEST(cells_v))` = 0 (mode 3).
- The authoritative `thermal.min_cell_voltage` (~3.0 V) was dropped by the bridge
  (mode 2) and unused by dashboards.
- A separate `diagnostics_low.cell_min_v` column existed but was never populated
  (mode 1).

Fix applied: bridge now emits `thermal.min_cell_voltage`/`max_cell_voltage`;
cell-V array stats skip zeros (bridge + retrospect SQL); the Orion HV panels read
the authoritative scalars.

## Prevention checklist

When you **add or rename a field** in `can_packets.proto`, update every copy:

- [ ] `stack/kafka/proto/sensor/sensor.proto` (Go bridge proto) and regenerate.
- [ ] `stack/ingest/protobuf/template.proto` if the legacy schema must track it.
- [ ] `analysis/sql_utils/models.py` + the matching `*_db_init.sql` column.
- [ ] `stack/kafka/cmd/bridge/main.go` `*ToMap()` — add the `m["field"] = ...` line.
- [ ] Dashboards that reference the old name (`analysis/database/dashboards/...`).
- [ ] If renaming proto vs column names, add a remap in
      `stack/ingest/mqtt_handler.py::_proto_decode` (see the wheel-speed remap).

When you **aggregate a repeated field** (`cells_v`, `cells_temps`):

- [ ] Exclude the `0.0` sentinel (`FILTER (WHERE value > 0)` / non-zero helper),
      or prefer an authoritative scalar field if one exists.

Recommended guardrail: a unit test that reflects over every leaf field of
`OrionSensorData` and asserts it appears in `orionToMap`'s output, so newly added
proto fields can't be silently dropped (mode 2).
