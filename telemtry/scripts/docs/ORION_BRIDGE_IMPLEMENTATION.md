# Orion Kafka Bridge Support

Implemented on March 1, 2026.

## Changes Overview

The Kafka Bridge (Go service) has been updated to support the **Orion** car type in addition to Nightwatch and Angelique. This ensures that Orion data received via gRPC from the ingest service is correctly processed and forwarded to Kafka for Grafana visualization.

### 1. Protobuf Definitions
- **File:** `stack/kafka/proto/sensor/sensor.proto`
  - Added `OrionSensorData`, `OrionDynamics`, `OrionControls`, `OrionPack`, `OrionDiagnosticsHigh`, `OrionDiagnosticsLow`, and `OrionThermal` message definitions.
  - These match the current schema used in the Python ingest service (`can_packets_pb2.py`).
- **File:** `stack/kafka/proto/bridge/bridge.proto`
  - Updated `SensorDataRequest` comment to include "Orion" as a valid `car_type`.

### 2. Bridge Logic (Go)
- **File:** `stack/kafka/cmd/bridge/main.go`
  - Updated `deserializeToJSON` to handle the `"Orion"` car type.
  - Implemented `orionToMap` function to translate the Orion protobuf message into a flat JSON map compatible with the Grafana Kafka datasource.
  - Mapped fields including Dynamics, Controls, Pack (with cell voltage/temp stats), Diagnostics, and Thermal data.

## Schema Driven Development (SDD) Note

**CRITICAL:** The files modified in this update are currently **NOT** managed by the automated Schema Driven Development (SDD) pipeline (`scripts/generate_schema.py` and `scripts/sync_schema.sh`).

- **Risk:** If the Orion protobuf schema changes (e.g., via `drivers/longhorn-lib`), these Go files must be updated manually.
- **Future Work:** The SDD system should be extended to:
  1. Generate Go protobuf code for the Kafka bridge directly from the master `.proto` source.
  2. Potentially generate the `*ToMap` functions in `main.go` to avoid manual synchronization of field mappings.

Until then, any changes to the Orion, Angelique, or Nightwatch schemas must be manually reflected in `stack/kafka/proto/sensor/sensor.proto` and the mapping logic in `stack/kafka/cmd/bridge/main.go`.
