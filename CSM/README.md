# Corner Sensor Module (CSM) Firmware

The Corner Sensor Module (CSM) collects the vehicle's ride height. There are four CSM boards located at the corners of the car.

CSM firmware will live in the `CSM/firmware` directory.

This directory can be recursively built by running `bazel build //CSM/...`. Additionally, the generated files will be placed in the `bazel-bin/CSM/` directory.

The firmware target is `//CSM/firmware:csm_firmware_2026`.
