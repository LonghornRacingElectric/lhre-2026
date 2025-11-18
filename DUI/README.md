# Driver User Interface (DUI) Board Firmware

The DUI board receives drive inputs from the steering wheel/dash and communicates vehicle status (IMD, BMS, etc) to driver.

DUI Firmware will live in the `DUI/firmware` directory.

This directory can be recursively built by running `bazel build //DUI/...`. Additionally, the generated files will be placed in the `bazel-bin/DUI/` directory.

The firmware target is //DUI/firmware:dui_firmware_2026.



