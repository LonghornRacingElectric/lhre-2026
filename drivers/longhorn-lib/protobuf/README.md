This folder holds a *workspace copy* of the generated protobuf schema derived from Longhorn CAN CSVs.

There are two related Bazel targets:

## 1) Bazel build output (recommended for builds)

Generates the schema as a normal Bazel output under `bazel-bin/`:

- `bazel build //drivers/longhorn-lib:can_proto`

Output path:

- `bazel-bin/drivers/longhorn-lib/can_packets.proto`

This automatically regenerates when `drivers/longhorn-lib/config/can_packets.csv` (or `can_bitfields.csv`) changes.

## 2) Workspace writer (useful for local dev / non-Bazel tooling)

Materializes the schema into this folder in the source tree:

- `bazel run //drivers/longhorn-lib:update_can_proto`

Output path:

- `drivers/longhorn-lib/protobuf/can_packets.proto`
