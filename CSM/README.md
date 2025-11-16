# Corner Sensor Module (CSM) Firmware and Model

The Corner Sensor Module (CSM) collects data from the vehicle suspension. There are four CSM boards located at the corners of the car.

CSM firmware will live in the `CSM/Firmware` directory, while the model files will be located in the `CSM/Model` directory.

This directory can be recursively built by running `bazel build //CSM/...`. Additionally, the generated files will be placed in the `bazel-bin/CSM/` directory.

The firmware target is `//CSM/Firmware:csm_firmware_2026`.

Model firmware target: `//CSM/Model:csm_model_2026`.
