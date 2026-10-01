# Wheel phase observer checks

`wheel_phase_test.c` exercises the HAL-free observer with synthetic signals for
Orion's confirmed four-sensor, 15-degree spacing and three-pole-pair geometry.
The production driver feeds this observer while retaining the legacy threshold
speed for existing telemetry. Control validity remains gated by calibration and
separately qualified acquisition and synchronization timing.

Run from the repository root:

```sh
bazel test //USM/tests:wheel_phase_test
```

A standalone compiler invocation avoids the firmware/Bazel toolchain:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I USM/firmware/usm USM/firmware/usm/wheel_phase.c \
  USM/tests/wheel_phase_test.c -lm -o /tmp/orion_wheel_phase_test
/tmp/orion_wheel_phase_test
```

In containers where LeakSanitizer cannot inspect `/proc`, run the binary with
`ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remain active.
The observer performs no dynamic allocation.

Covered cases include stationary and noisy stationary signals, offsets/gains/signs,
positive and negative speed, electrical phase and microsecond-clock wrapping,
variable sample intervals, acceleration from zero and reversal, acquisition
holdoff, CRC/status validity and freshness masks, repeated and backward timestamps,
excessive age and channel skew, long gaps, alias bounds, zero/large signal magnitude,
NaN/infinity, malformed configuration, pair disagreement, innovation rejection,
and reacquisition after faults. Synthetic tests do not validate installed sensor
calibration, magnets, noise, measured latency, or vehicle performance.

## Firmware integration contract

1. Fix and verify SPI mode, clock, chip selects, sensor initialization and response
   layout first. Configure deterministic acquisition and check each transaction,
   status, measurement counter and CRC before asserting validity/freshness.
2. Call `wheel_phase_default_config()`. Populate each channel's measured offset,
   amplitude scale and sign, and validate the pair phase relation throughout a
   rotation. Defaults intentionally have `calibrated=false`. A boolean flag is a
   commissioning gate, not evidence that calibration is correct.
3. Initialize `wheel_phase_t` with `wheel_phase_init()`. Configure the timing,
   magnitude, phase-noise, speed, disagreement and innovation bounds using measured
   data. Alpha/beta and the 10 ms default acquisition holdoff are initial tuning
   values, not guarantees of launch accuracy or sensor bandwidth.
4. Supply a complete set of four local radial fields and their acquisition times
   on the same `uint32_t` microsecond clock. `now_us` must use that clock too.
   All four `valid_mask` and `fresh_mask` bits must be set. Do not substitute SPI
   completion time for an unknown conversion time or mark repeated data fresh.
5. Call `wheel_phase_update()` once per complete fresh set. Only publish speed as
   usable when `state.output.valid` is true. The estimate is signed mechanical
   rad/s. `electrical_phase_rad` repeats three times per wheel revolution;
   `relative_angle_rad` resets on reacquisition and is not an absolute wheel angle.
6. Propagate validity and acquisition time over CAN. Consumers must independently
   expire output when no update arrives. The observer cannot detect a stopped
   producer when it is not called. Initialization, any rejected sample, and a
   dropped timing bound require reacquisition; old speed is never kept valid.

Pairs are `(0,2)` and `(1,3)`. The default correction adds 45 electrical degrees to
the second pair, assuming local signals `cos(phi - sensor_position * 3)`; verify
this sign convention against the installed geometry and positive car direction.
Pair-midpoint extrapolation uses the previous velocity to reduce inter-pair skew.
It does not correct skew between the two axes inside a pair. Simultaneous capture
and the configured channel-skew bound remain necessary.

The unwrap bound assumes actual speed remains inside `max_speed_rad_s`.
Unobserved full electrical turns cannot be detected from periodic fields alone;
set the bound above all physically reachable speeds and ensure acquisition gaps
cannot permit half an electrical revolution. Pair agreement also cannot detect a
common error affecting both pairs. `quality` is a diagnostic consistency score,
not a calibrated uncertainty or probability of correctness.

Frame integrity checks can also run without Bazel:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -I USM/firmware/usm \
  USM/firmware/usm/mlx90395_frame.c USM/tests/mlx90395_frame_test.c \
  -o /tmp/orion_mlx_frame_test
/tmp/orion_mlx_frame_test
```

See `VCU/model/docs/orion_traction_implementation.md` for the actual acquisition
and clock qualification gates; the current sequential read timestamps exceed
the observer's default channel-skew bound.
