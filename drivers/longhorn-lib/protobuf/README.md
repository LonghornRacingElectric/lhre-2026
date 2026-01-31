This folder is intended to hold generated protobuf schemas derived from the CAN definition CSV.

Generate/update the schema with:

- `bazel run //drivers/longhorn-lib:update_can_proto`

The generated output is:

- `drivers/longhorn-lib/protobuf/can_packets.proto`
