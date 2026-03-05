# Thermal Sensor Module (TSM) Firmware and Model

The Thermal Sensor Module (TSM) measures the ambient temperature near the radiator such that the radiator fan speeds can be properly set and controlled. It works in tandem with sensors measuring the cooling loop flow rate and cooling fluid temperature.

TSM Firmware will live in the `TSM/firmware` directory.

This directory can be recursively built by running `bazel build //TSM/...`. Additionally, the generated files will be placed in the `bazel-bin/TSM/` directory.

The firmware target is `//TSM/firmware:tsm_firmware_2026`.