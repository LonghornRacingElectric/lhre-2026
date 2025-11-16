# Vehicle Control Unit (VCU) Firmware and Model

The Vehicle Control Unit (VCU) manages the car's state and rules requirements. It also manages the entire torque path between
digitized driver pedal inputs and outputs.

VCU Firmware will live in the `VCU/firmware` directory, while the model files will be located in the `VCU/model` directory.

This directory can be recursively built by running `bazel build //VCU/...`. Additionally, the generated files will be placed in the `bazel-bin/VCU/` directory.

The firmware target is `//VCU/firmware:vcu_firmware_2026`.

Model firmware target: `//VCU/model:vcu_model_2026`.
