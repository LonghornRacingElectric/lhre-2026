# High Voltage Controller (HVC) Firmware and Model

The High Voltage Controller (HVC) monitors our Tractive System and controls its safety logic (interlocks, IMD, RTML, AIL). Also is the hub for all BMB comms and cell balancing.

HVC Firmware will live in the `HVC/firmware` directory, while the model files will be located in the `HVC/model` directory.

This directory can be recursively built by running `bazel build //HVC/...`. Additionally, the generated files will be placed in the `bazel-bin/HVC/` directory.

The firmware target is `//HVC/firmware:hvc_firmware_2026`.

Model firmware target: `//HVC/model:hvc_model_2026`.
