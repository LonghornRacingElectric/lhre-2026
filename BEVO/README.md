# BEVO 🗣️🗣️🗣️

Board Emitting Vehicle Outputs (BEVO). The interface between the car and the world.

Will be used to connect to the Vehicle CAN interfaces and receive telemetry data.

## Run modes

- Hermetic (monorepo default): use Bazel targets under `//BEVO/...`
- Nonhermetic (local/embedded): use scripts in `BEVO/nonhermetic/`

See `BEVO/nonhermetic/README.md` for the local setup and runbook.
