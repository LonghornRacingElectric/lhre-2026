# BEVO Nonhermetic (Local/Embedded) Runbook

This directory provides a non-Bazel runtime for BEVO daemons while keeping the existing hermetic Bazel flow intact.

## Why this exists

- Hermetic/Bazel in monorepo remains unchanged for CI and reproducibility.
- Embedded hardware and quick local iteration can run directly from Cargo binaries.

## Files

- `assets/can_packets.proto` - vendored protobuf schema for nonhermetic builds
- `assets/can.json` - vendored CAN decode mapping used at runtime
- `setup_local_env.sh` - one-time/local setup: refresh local assets (when monorepo data is present) and build release binaries
- `sync_assets.sh` - refresh generated runtime assets tracked in source tree
- `run_mock_stack.sh` - run `cand + dashd + loggerd + mock_can` locally
- `run_full_mock_stack.sh` - run the *complete* stack with mock CAN data (includes `publishd`)
- `run_real_stack.sh` - run `publishd + cand + dashd + loggerd` against real CAN interface

## Prerequisites

- `cargo` / Rust toolchain
- `python3`
- `bazel` (used by `sync_assets.sh` for `protoc`)
- Optional for `cell.py`: install `BEVO/requirements.txt`

Sparse checkout note:

- Nonhermetic runtime/build defaults use files inside `BEVO/nonhermetic/assets`.
- If the rest of monorepo is present, `setup_local_env.sh` refreshes `assets/can.json` from source CSVs automatically.

## Quick start (nonhermetic)

From repo root:

```bash
bash BEVO/nonhermetic/setup_local_env.sh
# run only the CAN-logging stack:
bash BEVO/nonhermetic/run_mock_stack.sh
# or run the full mock stack (includes publishd):
bash BEVO/nonhermetic/run_full_mock_stack.sh
```

For real CAN hardware:

```bash
CAND_CAN_INTERFACE=can0 bash BEVO/nonhermetic/run_real_stack.sh
```

## Environment variables

### cand

- `CAND_USE_MOCK` (`1`/`0`)
- `CAND_CAN_INTERFACE` (default `can0`)
- `CAND_CAN_JSON_PATH` (default `BEVO/nonhermetic/assets/can.json`)
- `CAND_PUBLISH_HZ` (default `10`)

### publishd

- `PUBLISHD_MQTT_HOST` (default `192.168.1.109`)
- `PUBLISHD_MQTT_PORT` (default `1883`)
- `PUBLISHD_MQTT_CLIENT_ID` (default `BEVO-ORION`)
- `PUBLISHD_REQUIRE_SERVER_PACKET_ID` (`1` to wait for server packet id, default `0` in `run_real_stack.sh`)

## Keeping generated assets current

Whenever CAN CSV/proto schema changes:

```bash
bash BEVO/nonhermetic/sync_assets.sh
```

This refreshes local generated assets used by nonhermetic builds:

- `BEVO/nonhermetic/assets/can.json`
- `BEVO/sensor_data.desc`
- `BEVO/generated_mapping.rs`
