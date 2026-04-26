# VCU Power-Limit Feedforward and Trim PID

This document describes the VCU torque limiter implemented in
`VCU/model/components/TorqueMap.c`. The goal is to make requested torque obey
pack power and current limits while preserving the existing driver pedal path.

## Control Flow

The torque map runs once per `vcu_model_step()` after APPS and BSE have been
evaluated.

1. Validate tuning parameters and required CAN inputs.
2. Clamp APPS travel to `[0, 1]`.
3. Compute live measured power from inverter DC bus voltage and current:
   `measured_power_w = inverter_dc_bus_voltage_v * inverter_dc_bus_current_a`.
4. Estimate open-circuit pack voltage (`OCV`) from HVC pack voltage when
   available, otherwise inverter DC bus voltage. The OCV estimate only updates
   while absolute DC bus current is less than 1 A.
5. Compute the active electrical power limit:
   `min(power_limit_w, inverter_dc_bus_voltage_v * current_limit_a)`.
6. Convert the electrical power limit into a mechanical feedforward torque cap:
   `active_power_limit_w * motor_efficiency / motor_angular_velocity_rad_s`.
7. Apply low-voltage derate from estimated cell OCV:
   `((cell_ocv_v - 3.3 V) / 0.1 V)` clamped to `[0, 1]`.
8. Apply hard safety cuts. If live bus current exceeds `hard_current_cut_a` or
   live measured power exceeds `hard_power_cut_w`,
   commanded torque is set to zero for that control step.
9. Compute trim PID from power error:
   `power_error_w = active_power_limit_w - measured_power_w`.
10. Clamp trim to `+/- power_limit_trim_limit_nm` and add it to feedforward
    torque.
11. Output torque is:
    `accel_pedal_travel * available_torque_nm`.

The feedforward term should do most of the limiting. The trim PID uses live DC
bus voltage and current so it reacts immediately to overshoot. OCV is not used
for power tracking; it only feeds the low-voltage derate.

## Inputs

`vcu_inputs_t` fields used by this controller:

| Field | Meaning |
| --- | --- |
| `inverter_dc_bus_voltage_v` | Inverter DC link voltage in volts. Required. |
| `inverter_dc_bus_current_a` | Inverter DC link current in amps. Required; positive current is treated as discharge. |
| `motor_speed_rpm` | Motor speed in RPM. Required. Absolute value is used. |
| `inverter_power_valid` | True only when inverter voltage and current CAN packets are fresh. |
| `inverter_speed_valid` | True only when motor speed is fresh. |
| `battery_voltage_v` | HVC pack voltage in volts. Used for OCV estimate when valid. |
| `battery_status_valid` | True only when HVC pack status is fresh. |

If required inverter inputs are invalid, non-finite, or voltage is not positive,
the controller sets `torque_cmd = 0` and raises `power_limit_input_fault`.

## Parameters

All parameters live under `vcu_parameters_t.torque_map`.

| Parameter | Units | Purpose |
| --- | ---: | --- |
| `max_torque_nm` | Nm | Absolute torque ceiling before power limiting. |
| `power_limit_w` | W | Electrical pack power limit target. |
| `current_limit_a` | A | Current-derived power limit. Active limit is no greater than `Vbus * current_limit_a`. |
| `hard_current_cut_a` | A | Fail-closed current safety threshold. Must be greater than or equal to `current_limit_a`. |
| `hard_power_cut_w` | W | Fail-closed power safety threshold. Must be greater than or equal to `power_limit_w`. |
| `ocv_cell_count` | cells | Number of series cells used to convert pack OCV to cell OCV. |
| `ocv_lpf_time_constant_s` | s | OCV low-pass time constant while current is near zero. Set to 0 for no OCV filtering. |
| `power_limit_min_rpm` | RPM | Minimum RPM used in power-to-torque conversion to avoid huge low-speed torque. |
| `power_limit_trim_limit_nm` | Nm | Symmetric clamp on PID trim torque. |
| `power_limit_kp` | Nm/W | Proportional trim gain. Positive error increases available torque; negative error reduces it. |
| `power_limit_ki` | Nm/(W*s) | Integral trim gain. Anti-windup holds the integrator when trim is saturated farther in the same direction. |
| `power_limit_kd` | Nm/(W/s) | Derivative trim gain. Only rising measured power rate reduces torque. |
| `power_limit_motor_efficiency_rpm[]` | RPM | Strictly increasing RPM breakpoints for efficiency lookup. |
| `power_limit_motor_efficiency[]` | ratio | Motor efficiency values in `(0, 1]`; interpolated and clamped to `[0.50, 1.00]`. |

Current firmware defaults:

| Parameter | Default |
| --- | ---: |
| `max_torque_nm` | 220 Nm |
| `power_limit_w` | 80000 W |
| `current_limit_a` | 200 A |
| `hard_current_cut_a` | 240 A |
| `hard_power_cut_w` | 85000 W |
| `ocv_cell_count` | 130 |
| `ocv_lpf_time_constant_s` | 1.0 s |
| `power_limit_min_rpm` | 100 RPM |
| `power_limit_trim_limit_nm` | 20 Nm |
| `power_limit_kp` | 0.002 Nm/W |
| `power_limit_ki` | 0 |
| `power_limit_kd` | 0 |

## Functions

| Function | Purpose |
| --- | --- |
| `torque_map_init()` | Clears OCV and PID state. Called from `vcu_model_init()`. |
| `torque_map_evaluate()` | Main control step. Validates inputs, computes feedforward torque, applies PID trim, sets torque and debug/fault outputs. |
| `lpf_alpha_from_tau()` | Converts the OCV time constant and step time into first-order low-pass alpha. |
| `lpf_step()` | Runs one OCV low-pass filter update. |
| `torque_map_efficiency_map_is_valid()` | Confirms RPM breakpoints increase and efficiencies are finite and in range. |
| `torque_map_params_are_valid()` | Confirms all power-limit parameters are finite and safe. |
| `motor_efficiency_at_rpm()` | Interpolates motor efficiency at the current absolute motor RPM. |
| `torque_from_power()` | Converts mechanical power to torque using RPM clamped by `power_limit_min_rpm`. |
| `vcu_can_set_powertrain_inputs()` | Firmware CAN adapter that fills inverter voltage/current/speed and HVC pack status freshness flags. |

## Debug Outputs

`vcu_outputs_t.debug` exposes:

| Field | Meaning |
| --- | --- |
| `ocv_estimate_v` | Current pack OCV estimate. |
| `active_power_limit_w` | Active electrical power target after current limit is applied. |
| `measured_power_w` | Raw `Vbus * Ibus`. |
| `low_voltage_derate_pct` | OCV-based torque derate percentage. |
| `power_limit_feedforward_torque_nm` | Power/speed-derived torque cap before PID trim. |
| `power_limit_feedback_p_nm` | Proportional trim contribution. |
| `power_limit_feedback_i_nm` | Integral trim contribution. |
| `power_limit_feedback_d_nm` | Derivative trim contribution. |
| `power_limit_feedback_torque_nm` | Total clamped PID trim. |
| `power_limit_available_torque_nm` | Final torque available before pedal scaling. |
| `motor_efficiency` | Interpolated efficiency used for feedforward. |

## Tuning Procedure

1. Start with `power_limit_ki = 0` and `power_limit_kd = 0`.
2. Verify CAN freshness. `power_limit_input_fault` must remain false during
   normal operation.
3. Set `power_limit_w` below the desired final value for low-risk testing.
4. Tune the efficiency map so steady-state measured power is close to the
   active limit with `power_limit_kp = 0`.
5. Increase `power_limit_kp` until measured power trims back toward the target
   without oscillation. The default `0.002 Nm/W` means a 5 kW overshoot removes
   about 10 Nm.
6. Add a small `power_limit_ki` only if steady-state error remains after the
   efficiency map and proportional gain are reasonable.
7. Use `power_limit_kd` only if power spikes rise too quickly. It reacts only
   to positive measured-power slope, so it cannot add torque.
8. Keep `hard_power_cut_w` and `hard_current_cut_a` above the control targets
   but below hardware or rules limits.

## Verification

The implementation is covered by model tests for:

- Existing pedal-to-torque behavior when the power limit is not active.
- Feedforward torque cap from power and speed.
- Proportional trim reducing torque when measured power is above target.
- Fail-closed behavior on stale/invalid power inputs.
- Fail-closed behavior on hard power cut.

The embedded VCU firmware target also builds with the CAN wiring in place.
