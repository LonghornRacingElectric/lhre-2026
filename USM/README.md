# Upright Sensor Module (USM) Firmware

The Upright Sensor Board collects and processes wheel-area data from the car’s unsprung. It reads wheel-speed information from three differential Hall-effect sensors and transmits data over CAN.

USM Firmware will live in the `USM/firmware` directory.

This directory can be recursively built by running `bazel build //USM/...`.

The firmware targets are `//USM/firmware:usm_firmware_2026_FL` (or `_FR`,
`_RL`, `_RR`, matching the installed corner).

## USB serial debugging

Build and flash the updated firmware for your board's corner, then connect the
board's USB data port. The debug stream uses USB CDC ("Upright Sensor Module"),
not the USART1 TX/RX pins. Open its serial port at 115200 baud, 8N1, no flow
control. USB CDC does not use the selected baud rate to time transmission.
On macOS, find the port with `ls /dev/cu.usbmodem*`, then open it with
`screen /dev/cu.usbmodemYOUR_PORT 115200`. Exit screen with Ctrl-A, then K, then Y.
Close the monitor before flashing again. On Linux use `/dev/ttyACM*`; on Windows
select the board's COM port in a serial terminal.

Two labeled rows alternate every 50 ms (each row approximately 10 Hz), alongside
existing CAN diagnostics. Logging uses a nonblocking queue and drops messages
when USB is disconnected/busy or the queue is full. This is a decimated bench
debug stream, not a full-rate calibration recording. Each row has its own logger
timestamp in RTOS ticks (currently milliseconds); adjacent rows are separate
samples. Sensor polling keeps its existing one-tick schedule.

| Row / field | Meaning |
| --- | --- |
| `USM ws`, `rpm` | Signed sin/cos estimate in mechanical rad/s and RPM; `nan` until valid. |
| `valid`, `status` | Estimator speed validity and hexadecimal rejection/status mask. |
| `legacy` | Existing threshold-based wheel speed in rad/s, still used by legacy CAN telemetry. This value has no new validity guarantee. |
| `imu_init`, `imu_read` | IMU status: 0 = success, -1 = SPI failure, -2 = wrong device ID (including an all-zero response). When initialization fails, `imu_read` repeats that error and no read is attempted. |
| `ax`, `ay`, `az` | IMU sensor-axis acceleration in m/s², including gravity; `nan` if initialization or the current read failed. These are not transformed into vehicle axes. |
| `HALL init`, `fresh` | Bit masks for initialized sensors and accepted fresh measurements in this poll. Bits 0–3 correspond to sensors 0–3; `0xf` means all four. Missing freshness can indicate a repeated conversion, invalid frame, reset, or SPI error. |
| `r0`–`r3` | Latest unfiltered local radial fields used by sin/cos processing, using the driver's existing raw-count × 0.00714 scale. Values are retained when a channel is not fresh; check `fresh` before using them. |
| `phase_valid`, `phase` | Phase validity and wrapped electrical phase in radians (three electrical revolutions per wheel revolution). |
| `pair_err`, `innov`, `q` | Pair disagreement and observer innovation in radians, plus diagnostic quality from 0 to 1. Phase diagnostics are only meaningful with their validity/status flags. |

`status` bits: `0x001` uncalibrated, `0x002` bad configuration, `0x004` bad
sample, `0x008` stale, `0x010` bad time, `0x020` channel skew, `0x040` bad
magnitude, `0x080` pair disagreement, `0x100` alias risk, `0x200` sample gap,
`0x400` innovation rejection, `0x800` acquiring.

The checked-in `usm/wheel_calibration.h` intentionally sets `calibrated=false`.
Expect `valid=0 status=0x001 ws=nan` until measured calibration is supplied;
Hall fields, legacy speed, and IMU data remain available for debugging. Do not
enable calibration just to make a speed appear. Acquisition timing also needs
qualification; see [the observer checks](tests/README.md).
