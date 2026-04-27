# VCU Traction Control

This document describes the traction-control system on
`Andrew's-Playground-2026`, implemented in
`VCU/model/components/TractionControl.c`.

Traction control is a downstream torque clamp. It receives an already-computed
`out->torque_cmd`, estimates driven-wheel slip, and only reduces that torque
when slip exceeds the target. It never adds torque. It is deliberately
fail-open: when disabled or when required TC inputs are invalid, it leaves the
incoming torque request unchanged and raises `tc_input_fault` when appropriate.

## Control Flow

1. Save the incoming torque command as `unregulated_torque_nm`.
2. If `tc_disable` is true, or wheel radius/final-drive ratio are invalid, pass
   torque through unchanged.
3. Validate all traction-control parameters. This is a numeric sanity check, not
   a driving-state check. The code confirms every required gain, threshold, time
   constant, and geometry parameter is finite and within a physically usable
   range before allowing TC to clamp torque.
4. Require fresh wheel-speed inputs. If `wheel_speeds_valid` is false, set
   `tc_input_fault`, reset the slip integrator, and pass torque through.
5. Convert wheel speeds from rad/s to vehicle speeds using
   `tc_wheel_radius_m`.
6. Convert motor speed to rear driven speed using `tc_final_drive_ratio`.
7. Validate front, rear, and motor-derived speeds against range, acceleration,
   and disagreement thresholds:
   - Each wheel-derived speed must be finite, non-negative, and less than or
     equal to `tc_max_wheel_speed_mps`.
   - Front wheel speeds also get an acceleration plausibility check against the
     previous valid front speed. A front speed jump is rejected if it requires
     more than `tc_max_reference_accel_mps2`.
   - Motor-derived speed is valid only if inverter speed is fresh, final drive
     ratio is positive, and the converted speed is within the max-speed limit.
   - FL/FR disagreement is checked against `tc_front_disagreement_mps`.
   - RL/RR disagreement is checked against `tc_rear_disagreement_mps`.
   - Motor-derived rear speed disagreement is checked against
     `tc_motor_rear_disagreement_mps`.
8. Estimate reference vehicle speed from the non-driven front wheels.
9. Estimate driven speed from rear wheel speeds and motor speed.
10. Optionally blend reference speed with longitudinal-accel prediction. If
    acceleration is enabled and valid, the code predicts vehicle speed from the
    previous filtered speed plus longitudinal acceleration over `dt_s`, then
    blends that prediction with the front-wheel measured speed using
    `tc_reference_accel_blend`.
11. Low-pass filter reference speed and driven speed.
12. Compute slip ratio:
    `(driven_speed - vehicle_speed) / max(vehicle_speed, tc_min_vehicle_speed_mps)`.
    Negative slip is clamped to zero.
13. Low-pass filter slip ratio.
14. Compute target slip from base/min/max slip, driver adjustment, lateral
    usage, and optional aero lateral capacity.
15. Compute slip error above target, then apply `tc_slip_hysteresis`. The raw
    error is `filtered_slip_ratio - target_slip`. If that raw error is less than
    the hysteresis band, the controller treats it as zero. If it is above the
    band, the controller subtracts the hysteresis value before calculating
    torque reduction. This creates a deadband around the target so TC does not
    chatter when slip is very close to the desired value.
16. Low-pass filter slip error for feedback.
17. Compute slip rate and driven-wheel excess acceleration.
18. If vehicle speed and requested torque are above enable thresholds, compute
    torque reduction from P + I + D + driven-accel terms.
19. Clamp reduction by `tc_max_torque_reduction_nm` and the current torque
    request.
20. Slew the torque limit: fast cuts use `tc_cut_slew_nm_per_s`, recovery uses
    `tc_recovery_slew_nm_per_s`.
21. Set `out->torque_cmd = min(unregulated_torque_nm, torque_limit_nm)`.

## Inputs

`vcu_inputs_t` fields used by TC:

| Field | Meaning |
| --- | --- |
| `wheel_speed_fl_rad_s` | Front-left wheel angular speed. Used for reference vehicle speed. |
| `wheel_speed_fr_rad_s` | Front-right wheel angular speed. Used for reference vehicle speed. |
| `wheel_speed_rl_rad_s` | Rear-left wheel angular speed. Used for driven speed. |
| `wheel_speed_rr_rad_s` | Rear-right wheel angular speed. Used for driven speed. |
| `wheel_speeds_valid` | Required freshness flag for all wheel-speed data. If false, TC fails open with `tc_input_fault`. |
| `motor_speed_rpm` | Motor speed used as an additional driven-speed estimate. |
| `inverter_speed_valid` | Freshness flag for motor speed. If false, wheel rear speeds can still provide driven speed. |
| `longitudinal_accel_mps2` | Optional longitudinal acceleration. Used only when `tc_use_accel` and `accel_valid` are true. |
| `lateral_accel_mps2` | Optional lateral acceleration. Used only when `tc_use_accel` and `accel_valid` are true. |
| `accel_valid` | Freshness/plausibility flag for acceleration inputs. |
| `tc_longitudinal_adjust` | Optional driver trim added to `tc_longitudinal_adjust` when `tc_driver_adjust_valid` is true. |
| `tc_lateral_adjust` | Optional driver trim added to `tc_lateral_adjust` when `tc_driver_adjust_valid` is true. |
| `tc_driver_adjust_valid` | Enables driver longitudinal/lateral adjustment inputs. |

## Parameters

All parameters are in `vcu_parameters_t.traction_control`.

| Parameter | Units | Purpose |
| --- | ---: | --- |
| `tc_disable` | bool | Master disable. When true, TC passes torque through and does not set TC faults. |
| `tc_use_accel` | bool | Enables accelerometer usage for lateral slip tightening and longitudinal speed prediction. |
| `tc_aero_lateral_limit_enable` | bool | Enables speed-squared aero addition to the lateral acceleration limit. |
| `tc_wheel_radius_m` | m | Converts wheel rad/s to ground speed. Must be positive. |
| `tc_final_drive_ratio` | ratio | Converts motor speed to rear driven speed. Must be positive. |
| `tc_longitudinal_adjust` | unitless | Base slip-target adjust. `-1` moves to min slip, `+1` moves to max slip. |
| `tc_lateral_adjust` | unitless | Scales lateral slip reduction. `-1` reduces lateral tightening, `+1` increases it. |
| `tc_base_target_slip` | slip ratio | Nominal target slip. Must be between min and max target slip. |
| `tc_min_target_slip` | slip ratio | Most conservative target slip. Must be positive. |
| `tc_max_target_slip` | slip ratio | Most permissive target slip. Must be >= base target. |
| `tc_slip_hysteresis` | slip ratio | Deadband above target before TC starts reducing torque. |
| `tc_lateral_accel_limit_mps2` | m/s^2 | Baseline lateral acceleration capacity used to calculate lateral usage. |
| `tc_aero_lateral_accel_gain_per_mps2` | 1/m | Adds `gain * vehicle_speed^2` to lateral limit when aero mode is enabled. |
| `tc_lateral_slip_reduction_gain` | unitless | Strength of target-slip reduction as lateral usage rises. |
| `tc_min_vehicle_speed_mps` | m/s | Minimum speed for TC enable and denominator floor for slip calculation. |
| `tc_min_torque_nm` | Nm | Minimum requested torque for TC intervention. |
| `tc_max_wheel_speed_mps` | m/s | Rejects wheel or motor-derived speeds above this value. This catches sensor scaling errors, missing sign handling, bus corruption, and impossible spikes before they can create a false slip estimate. |
| `tc_max_reference_accel_mps2` | m/s^2 | Max allowed front reference-speed acceleration between samples. |
| `tc_front_disagreement_mps` | m/s | Max allowed FL/FR disagreement before flagging `TC_SENSOR_FRONT_DISAGREE`. |
| `tc_rear_disagreement_mps` | m/s | Max allowed RL/RR disagreement before flagging `TC_SENSOR_REAR_DISAGREE`. |
| `tc_motor_rear_disagreement_mps` | m/s | Max allowed motor/rear-wheel speed disagreement. |
| `tc_speed_lpf_time_constant_s` | s | Low-pass time constant for reference and driven speeds. Set to 0 for no filtering. |
| `tc_slip_lpf_time_constant_s` | s | Low-pass time constant for slip ratio. Set to 0 for no filtering. |
| `tc_feedback_lpf_time_constant_s` | s | Low-pass time constant for slip error used by feedback. Set to 0 for no filtering. |
| `tc_reference_accel_blend` | ratio | Blend between wheel-measured reference speed and accel-predicted speed. `0` uses wheels only, `1` uses prediction only. |
| `tc_kp_nm_per_slip` | Nm/slip | Proportional torque reduction gain from filtered slip error. |
| `tc_ki_nm_per_slip_s` | Nm/(slip*s) | Integral torque reduction gain. |
| `tc_kd_nm_per_slip_rate` | Nm/(slip/s) | Derivative gain. Uses only positive slip-rate. |
| `tc_driven_accel_gain_nm_per_mps2` | Nm/(m/s^2) | Torque reduction from driven-wheel acceleration above vehicle acceleration. |
| `tc_integral_limit_nm` | Nm | Maximum integral contribution after multiplying by `tc_ki_nm_per_slip_s`. |
| `tc_max_torque_reduction_nm` | Nm | Absolute cap on torque removed by TC. |
| `tc_cut_slew_nm_per_s` | Nm/s | Maximum rate for lowering the torque limit. Higher value cuts faster. |
| `tc_recovery_slew_nm_per_s` | Nm/s | Maximum rate for raising the torque limit after slip recovers. |

## Andrew's Playground Defaults

The branch-level firmware constants in `VCU/firmware/Core/Src/app_freertos.c`
set:

| Parameter | Default |
| --- | ---: |
| `tc_disable` | true |
| `tc_use_accel` | false |
| `tc_aero_lateral_limit_enable` | false |
| `tc_wheel_radius_m` | 0.2 m |
| `tc_final_drive_ratio` | 3.307 |
| `tc_longitudinal_adjust` | 0.0 |
| `tc_lateral_adjust` | 0.0 |
| `tc_base_target_slip` | 0.08 |
| `tc_min_target_slip` | 0.03 |
| `tc_max_target_slip` | 0.16 |
| `tc_slip_hysteresis` | 0.01 |
| `tc_lateral_accel_limit_mps2` | 11.0 m/s^2 |
| `tc_aero_lateral_accel_gain_per_mps2` | 0.0025 |
| `tc_lateral_slip_reduction_gain` | 1.0 |
| `tc_min_vehicle_speed_mps` | 3.0 m/s |
| `tc_min_torque_nm` | 5.0 Nm |
| `tc_max_wheel_speed_mps` | 90.0 m/s |
| `tc_max_reference_accel_mps2` | 35.0 m/s^2 |
| `tc_front_disagreement_mps` | 3.0 m/s |
| `tc_rear_disagreement_mps` | 8.0 m/s |
| `tc_motor_rear_disagreement_mps` | 8.0 m/s |
| `tc_speed_lpf_time_constant_s` | 0.035 s |
| `tc_slip_lpf_time_constant_s` | 0.020 s |
| `tc_feedback_lpf_time_constant_s` | 0.025 s |
| `tc_reference_accel_blend` | 0.20 |
| `tc_kp_nm_per_slip` | 900.0 |
| `tc_ki_nm_per_slip_s` | 80.0 |
| `tc_kd_nm_per_slip_rate` | 25.0 |
| `tc_driven_accel_gain_nm_per_mps2` | 2.0 |
| `tc_integral_limit_nm` | 40.0 Nm |
| `tc_max_torque_reduction_nm` | 220.0 Nm |
| `tc_cut_slew_nm_per_s` | 2500.0 Nm/s |
| `tc_recovery_slew_nm_per_s` | 350.0 Nm/s |

`tc_disable` is true in the branch defaults, so the code is configured for
bring-up but not active until that flag is changed.

## Speed Estimation Logic

The max-speed check exists because TC can only be as good as its speed
estimate. An implausibly high wheel speed can make the controller believe the
car has huge driven slip and unnecessarily cut torque, or make a front reference
speed jump so high that real slip is hidden. `tc_max_wheel_speed_mps` should be
set above any physically reachable vehicle speed, with margin, but below values
that clearly indicate a sensor or decode problem.

Front wheel speeds are the reference vehicle speed because they are non-driven.
Each front wheel is checked for:

- finite, non-negative speed
- speed below `tc_max_wheel_speed_mps`
- acceleration from prior valid sample below `tc_max_reference_accel_mps2`

If both front wheels are valid and agree within `tc_front_disagreement_mps`, TC
uses their average. If both are valid but disagree, TC flags
`TC_SENSOR_FRONT_DISAGREE` and chooses the speed closer to the previous filtered
vehicle-speed prediction. If only one front wheel is valid, TC uses that one. If
neither is valid, TC flags `TC_SENSOR_NO_REFERENCE_SPEED` and fails open.

Driven speed comes from rear wheels and motor speed:

- Rear wheel speeds are range-checked against `tc_max_wheel_speed_mps`.
- If both rear wheels are valid and agree within `tc_rear_disagreement_mps`, TC
  uses the higher rear speed.
- If rear wheels disagree, TC flags `TC_SENSOR_REAR_DISAGREE`. It normally uses
  the higher rear speed, unless the higher rear speed is also an outlier against
  motor-derived speed while the lower rear speed agrees with motor-derived
  speed.
- Motor speed is converted to ground speed with wheel radius and final-drive
  ratio. If valid, it is blended into driven-speed selection by taking the max
  of rear-wheel-derived speed and motor-derived speed.
- If no rear wheel and no motor speed are valid, TC flags
  `TC_SENSOR_NO_DRIVEN_SPEED` and fails open.

The rear disagreement logic above is the current Andrew's branch behavior. For
initial bring-up, this is more complicated than necessary and it makes one weak
assumption: with an open differential, motor speed represents the average
driveline speed, not necessarily the speed of the left or right rear wheel. That
means choosing the rear wheel "closer" to motor speed can reject the wrong wheel
in a one-wheel-slip event.

Recommended initial strategy:

1. Keep the front reference plausibility logic.
2. Use the larger valid rear wheel speed as driven speed when rear wheel speeds
   are available. This is conservative for traction control because the faster
   driven wheel is the one most likely to be spinning.
3. Use motor-derived speed as a diagnostic and as a fallback only when rear
   wheel speeds are unavailable or invalid.
4. Keep `TC_SENSOR_MOTOR_REAR_DISAGREE` as telemetry. Do not use motor closeness
   to choose between disagreeing rear wheels until differential behavior is
   validated with logged data.

This keeps the controller easier to reason about while preserving the useful
front plausibility, lateral scaling, and torque-clamp behavior.

## Slip Target Logic

Base target slip starts at `tc_base_target_slip`.

Longitudinal adjustment moves the target:

- `0.0`: use base target
- positive: interpolate toward `tc_max_target_slip`
- negative: interpolate toward `tc_min_target_slip`

If `tc_driver_adjust_valid` is true, `in->tc_longitudinal_adjust` is added to
the configured `tc_longitudinal_adjust`, then clamped to `[-1, 1]`.

Lateral acceleration can tighten the target when `tc_use_accel` and
`accel_valid` are true. Lateral usage is:

`abs(lateral_accel_mps2) / lateral_accel_limit`

When `tc_aero_lateral_limit_enable` is true:

`lateral_accel_limit = tc_lateral_accel_limit_mps2 + tc_aero_lateral_accel_gain_per_mps2 * vehicle_speed^2`

The lateral reduction term is:

`lateral_usage^2 * tc_lateral_slip_reduction_gain * (1 + lateral_adjust)`

That reduction interpolates the target toward `tc_min_target_slip`. Driver
lateral adjustment is added the same way as longitudinal adjustment.

## Torque Reduction Logic

Slip is:

`max((driven_speed - vehicle_speed) / max(vehicle_speed, tc_min_vehicle_speed_mps), 0)`

Slip error is:

`filtered_slip_ratio - target_slip`

If slip error is below `tc_slip_hysteresis`, it is set to zero. Otherwise the
hysteresis value is subtracted from the error.

TC only computes torque reduction when:

- filtered vehicle speed is at least `tc_min_vehicle_speed_mps`
- incoming torque is at least `tc_min_torque_nm`

The raw torque reduction is:

`P + I + D + accel`

Where:

- `P = tc_kp_nm_per_slip * filtered_slip_error`
- `I = tc_ki_nm_per_slip_s * slip_integral`
- `D = tc_kd_nm_per_slip_rate * max(slip_rate, 0)`
- `accel = tc_driven_accel_gain_nm_per_mps2 * max(driven_accel - vehicle_accel, 0)`

The integrator is clamped so its contribution cannot exceed
`tc_integral_limit_nm`. If `tc_ki_nm_per_slip_s` is effectively zero, the
integrator is reset to zero.

Raw reduction is clamped to:

`[0, min(tc_max_torque_reduction_nm, unregulated_torque_nm)]`

Then a target torque limit is computed:

`target_torque_limit_nm = unregulated_torque_nm - torque_reduction_nm`

The stateful torque limit slews toward that target:

- lowering the torque limit uses `tc_cut_slew_nm_per_s`
- raising the torque limit uses `tc_recovery_slew_nm_per_s`

The final output is:

`out->torque_cmd = min(unregulated_torque_nm, torque_limit_nm)`

This structure is broadly similar to a motorsport-style TC architecture:

- use non-driven wheels for vehicle reference speed
- calculate driven slip against a target slip
- vary the target slip with driver/setup knobs and lateral usage
- reduce torque with a fast path for cuts and a slower path for recovery
- expose enough telemetry to tune from logged data

The part that is less appropriate for an initial implementation is the rear
wheel/motor outlier selection described above. Motorsport systems often have
more validated sensor models, tire models, and differential-specific logic. For
our first usable version, the safer engineering choice is conservative rear
speed selection plus clear logging, then add complexity only when data shows it
improves control.

## Faults and Sensor Flags

`out->faults.tc_input_fault` is set when TC is enabled but required parameters
or required sensor paths are invalid. TC still passes torque through when this
happens.

Sensor fault flags are exposed as `out->debug.tc_sensor_fault_flags`:

| Flag | Meaning |
| --- | --- |
| `TC_SENSOR_FRONT_LEFT_INVALID` | Front-left wheel speed is out of range or implausible. |
| `TC_SENSOR_FRONT_RIGHT_INVALID` | Front-right wheel speed is out of range or implausible. |
| `TC_SENSOR_REAR_LEFT_INVALID` | Rear-left wheel speed is out of range. |
| `TC_SENSOR_REAR_RIGHT_INVALID` | Rear-right wheel speed is out of range. |
| `TC_SENSOR_MOTOR_SPEED_INVALID` | Motor-derived driven speed is unavailable or implausible. |
| `TC_SENSOR_FRONT_DISAGREE` | Front wheels disagree beyond threshold. |
| `TC_SENSOR_REAR_DISAGREE` | Rear wheels disagree beyond threshold. |
| `TC_SENSOR_MOTOR_REAR_DISAGREE` | Motor-derived speed disagrees with rear-wheel-derived speed. |
| `TC_SENSOR_REFERENCE_ACCEL_IMPLAUS` | Front reference speed jumped faster than allowed. |
| `TC_SENSOR_NO_REFERENCE_SPEED` | No valid front reference speed is available. |
| `TC_SENSOR_NO_DRIVEN_SPEED` | No valid driven speed is available. |

## Outputs and Debug Data

TC modifies:

| Output | Meaning |
| --- | --- |
| `out->torque_cmd` | Final torque request after TC clamp. Never greater than incoming torque. |
| `out->faults.tc_input_fault` | TC input/parameter fault. Fail-open, not torque-cutting by itself. |

Debug outputs:

| Debug field | Meaning |
| --- | --- |
| `tc_active` | True when TC is controlling and the applied clamp is meaningfully reducing torque. |
| `tc_vehicle_speed_mps` | Filtered reference vehicle speed. |
| `tc_driven_speed_mps` | Filtered driven speed. |
| `tc_slip_ratio` | Filtered driven slip ratio. |
| `tc_target_slip_ratio` | Current target slip. |
| `tc_slip_error` | Filtered slip error after hysteresis. |
| `tc_torque_reduction_nm` | Difference between incoming torque and final torque. |
| `tc_torque_limit_nm` | Stateful slewed torque limit. |
| `tc_lateral_usage` | Lateral acceleration usage ratio `[0, 1]`. |
| `tc_lateral_accel_limit_mps2` | Active lateral acceleration limit. |
| `tc_longitudinal_accel_mps2` | Measured or speed-derived vehicle longitudinal acceleration. |
| `tc_lateral_accel_mps2` | Lateral acceleration actually used by TC. |
| `tc_front_disagreement_mps` | Absolute FL/FR disagreement. |
| `tc_rear_disagreement_mps` | Absolute RL/RR disagreement. |
| `tc_motor_rear_disagreement_mps` | Absolute motor/rear driven-speed disagreement. |
| `tc_sensor_fault_flags` | Bitmask of TC sensor flags. |

## Tuning Order

1. Keep `tc_disable = true` until wheel-speed polarity, scaling, and CAN
   freshness are verified.
2. Validate `tc_wheel_radius_m` and `tc_final_drive_ratio` by comparing front
   speed, rear speed, and motor-derived speed during straight-line coast.
3. Tune plausibility thresholds before enabling torque reduction:
   `tc_max_wheel_speed_mps`, `tc_front_disagreement_mps`,
   `tc_rear_disagreement_mps`, `tc_motor_rear_disagreement_mps`, and
   `tc_max_reference_accel_mps2`.
4. Start with `tc_use_accel = false` and
   `tc_aero_lateral_limit_enable = false`.
5. Set slip targets: begin with conservative `tc_base_target_slip`, then use
   `tc_min_target_slip` and `tc_max_target_slip` to define the driver-adjustable
   range.
6. Tune `tc_kp_nm_per_slip` first. Keep `tc_ki_nm_per_slip_s` and
   `tc_kd_nm_per_slip_rate` at zero until proportional response is understood.
7. Tune `tc_cut_slew_nm_per_s` for intervention speed and
   `tc_recovery_slew_nm_per_s` for how gently torque returns.
8. Add small integral only if steady slip remains after proportional tuning.
9. Add derivative and driven-accel gain only if slip onset is too abrupt.
10. Enable accel-based lateral tightening only after acceleration axes and signs
    are validated.

## Verification Coverage on Andrew's Branch

The branch includes tests for:

- disabled TC passing torque through
- missing wheel speeds failing open
- implausible reference speeds failing open
- matched driven/reference speeds passing torque
- front disagreement choosing the plausible reference
- implausible reference spikes not becoming valid history
- excess driven speed reducing torque
- lateral acceleration tightening target slip
- aero lateral limit reducing high-speed corner intervention
- trace replay showing cut and recovery behavior
