# Upright Sensor Module (USM) Firmware

The Upright Sensor Board collects and processes wheel-area data from the car’s unsprung. It reads wheel-speed information from three differential Hall-effect sensors and transmits data over CAN.

USM Firmware will live in the `USM/firmware` directory.

This directory can be recursively built by running `bazel build //USM/...`.

The firmware target is `//USM/firmware:usm_firmware_2026`.
