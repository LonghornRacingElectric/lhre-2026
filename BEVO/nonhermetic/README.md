# BEVO Nonhermetic (Local/Embedded) Runbook

This directory provides a non-Bazel runtime for BEVO daemons while keeping the existing hermetic Bazel flow intact.

## Why this exists

- Hermetic/Bazel in monorepo remains unchanged for CI and reproducibility.
- Embedded hardware and quick local iteration can run directly from Cargo binaries.

## Files

- `setup_local_env.sh` - one-time/local setup: generate CAN json and build release binaries
- `sync_assets.sh` - refresh generated runtime assets tracked in source tree
- `run_mock_stack.sh` - run `cand + dashd + loggerd + mock_can` locally
- `run_real_stack.sh` - run `publishd + cand + dashd + loggerd` against real CAN interface

## Prerequisites

- `cargo` / Rust toolchain
- `python3`
- `bazel` (used by `sync_assets.sh` for `protoc`)
- Optional for `cell.py`: install `BEVO/requirements.txt`

## Quick start (nonhermetic)

From repo root:

```bash
bash BEVO/nonhermetic/setup_local_env.sh
bash BEVO/nonhermetic/run_mock_stack.sh
```

For real CAN hardware:

```bash
CAND_CAN_INTERFACE=can0 bash BEVO/nonhermetic/run_real_stack.sh
```

## Environment variables

### cand

- `CAND_USE_MOCK` (`1`/`0`)
- `CAND_CAN_INTERFACE` (default `can0`)
- `CAND_CAN_JSON_PATH` (default `drivers/longhorn-lib/can.json`)
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

- `drivers/longhorn-lib/can.json`
- `BEVO/sensor_data.desc`
- `BEVO/generated_mapping.rs`
