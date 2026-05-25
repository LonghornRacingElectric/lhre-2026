# VCU Power-Limit Torque Table And Trim PID

This document describes the isolated VCU power-limit update in
`VCU/model/components/TorqueMap.c`.

The control goal is:

1. Use a calibrated 1D RPM-to-available-torque table for the pack power limit.
2. Shape the pedal with an exponent.
3. Let the pedal request a percentage of the available torque at the current
   RPM.
4. Use live inverter electrical power to trim the available torque.
5. Fail closed on invalid inputs, invalid parameters, or hard power/current
   excursions.

## System State

The current branch removes the old standalone `PowerLimit` component because
that path had its trim disabled. The power-limit logic now lives in the torque
stage so the final drive torque path is easy to review:

```text
APPS pedal travel
  -> pedal exponent shaping
  -> 1D power-limit torque lookup by motor RPM
  -> current-limit scaling
  -> OCV low-voltage derate
  -> electrical-power PID trim
  -> pedal percentage of available torque
  -> PRNDL / fault final authority
```

No traction control, launch control, dashboard work, or unrelated playground
features are included.

## Main Logic

`torque_map_evaluate()` runs once per model step after APPS and BSE have been
evaluated.

1. Validate parameters.
   The controller requires finite, physically valid parameters. The torque table
   RPM breakpoints must be strictly increasing, table torque values must be
   non-negative and no greater than `max_torque_nm`, hard current must be at
   least the current target, and hard power must be at least the power target.

2. Validate live CAN inputs.
   `inverter_power_valid` and `inverter_speed_valid` must be true. Live DC bus
   voltage must be finite and positive. Live DC bus current and motor speed must
   be finite. Failure sets `power_limit_input_fault` and commands zero torque.

3. Shape pedal.
   APPS travel is clamped to `[0, 1]`, then shaped:

   ```text
   pedal_shaped_pct = pedal_raw_pct ^ pedal_exponent
   ```

   `pedal_exponent = 1.0` is linear. Values greater than 1 soften initial
   pedal response while still reaching 100% at full pedal.

4. Lookup available torque from RPM.
   Absolute motor RPM is used to interpolate the 1D power-limit torque table.
   The table is the nominal available torque for `power_limit_w`.

5. Compute measured electrical power.
   Live measured power is raw and unfiltered:

   ```text
   measured_power_w = inverter_dc_bus_voltage_v * inverter_dc_bus_current_a
   ```

   Bus current is not low-pass filtered. The trim loop and hard cuts react as
   quickly as CAN feedback and the 3 ms control task allow.

6. Apply current-derived power scaling.
   The active power target is:

   ```text
   active_power_limit_w = min(power_limit_w, Vbus * current_limit_a)
   power_limit_scale = active_power_limit_w / power_limit_w
   ```

   The 1D torque table is multiplied by `power_limit_scale`. This keeps the
   table usable when live voltage makes the current target more restrictive than
   the nominal power target.

7. Update OCV estimate for low-voltage derate only.
   OCV is initialized from HVC pack voltage when fresh, otherwise inverter DC
   bus voltage. After initialization, OCV only updates when absolute DC bus
   current is below 1 A. OCV is not used for power tracking.

8. Apply low-voltage derate.

   ```text
   cell_ocv_v = ocv_estimate_v / ocv_cell_count
   voltage_derate = clamp((cell_ocv_v - 3.3 V) / 0.1 V, 0.0, 1.0)
   ```

   This gives full torque at 3.4 V/cell and above, zero torque at 3.3 V/cell
   and below, and a linear ramp between them.

9. Apply hard cuts.
   If live DC bus current exceeds `hard_current_cut_a`, the controller commands
   zero torque for the step and sets `current_safety_cut`. If live measured
   power exceeds `hard_power_cut_w`, it commands zero torque and sets
   `power_safety_cut`.

10. Compute PID trim from live electrical power.

    ```text
    power_error_w = active_power_limit_w - measured_power_w
    trim_torque_nm = Kp * error + Ki * integral(error) - Kd * rising_power_rate
    ```

    Positive error means measured power is below target, so trim may add torque
    up to `power_limit_trim_limit_nm`. Negative error means measured power is
    above target, so trim removes torque immediately. The derivative term only
    reacts to rising measured power, so it cannot add torque. The integral term
    uses anti-windup and resets when pedal is nearly zero or the derated table
    torque is zero.

11. Compute available torque.

    ```text
    available_torque_nm =
      clamp(derated_table_torque_nm + trim_torque_nm,
            0,
            max_torque_nm * voltage_derate)
    ```

12. Command pedal percentage of available torque.

    ```text
    torque_cmd = pedal_shaped_pct * available_torque_nm
    ```

    PRNDL and existing APPS/BSE model faults can still force final torque to
    zero after this stage.

## 1D Power-Limit Torque Table

The torque table is calibrated from the EMRAX 228 high-voltage efficiency map
and the selected pack power target. The basic calculation used to create each
point is:

```text
torque_nm = min(max_torque_nm,
                power_limit_w * estimated_efficiency / motor_omega_rad_s)
```

The firmware default table is calibrated for `power_limit_w = 75,000 W`,
`max_torque_nm = 220 Nm`, and approximate efficiency values from the supplied
EMRAX efficiency plot. If `power_limit_w` is changed substantially, this table
should be recalculated from the same efficiency map.

| RPM | Available torque |
| ---: | ---: |
| 0 | 220 Nm |
| 500 | 220 Nm |
| 1000 | 220 Nm |
| 1500 | 220 Nm |
| 2000 | 220 Nm |
| 2500 | 220 Nm |
| 3000 | 220 Nm |
| 3500 | 192 Nm |
| 4000 | 166 Nm |
| 4500 | 145 Nm |
| 5000 | 129 Nm |
| 5500 | 115 Nm |
| 6000 | 103 Nm |

The controller interpolates linearly between points. Below the first point it
uses the first torque value. Above the last point it uses the last torque value.

## Inputs

| Input | Purpose |
| --- | --- |
| `accel_pedal_travel` | Produced by APPS; raw pedal percentage before exponent shaping. |
| `motor_speed_rpm` | Absolute value selects the 1D torque table point. |
| `inverter_dc_bus_voltage_v` | Live bus voltage for measured power and current-derived power target. |
| `inverter_dc_bus_current_a` | Live bus current for measured power, hard current cut, and OCV update inhibit. |
| `inverter_power_valid` | Must be true. False fails closed to zero torque. |
| `inverter_speed_valid` | Must be true. False fails closed to zero torque. |
| `battery_voltage_v` | HVC pack voltage for OCV when `battery_status_valid` is true. |
| `battery_status_valid` | Enables HVC pack voltage as OCV source. |

## Parameters

All parameters live under `vcu_parameters_t.torque_map`.

| Parameter | Units | Purpose |
| --- | ---: | --- |
| `max_torque_nm` | Nm | Absolute torque ceiling. Table values must not exceed this. |
| `pedal_exponent` | none | Pedal shaping exponent. `1.0` linear, `>1.0` softer initial pedal. |
| `power_limit_w` | W | Nominal electrical power target that the table is calibrated for. |
| `current_limit_a` | A | Current-derived power limit: `Vbus * current_limit_a`. |
| `hard_current_cut_a` | A | Immediate zero-torque current threshold. |
| `hard_power_cut_w` | W | Immediate zero-torque power threshold. |
| `ocv_cell_count` | cells | Series cell count used to convert pack OCV to cell OCV. |
| `ocv_lpf_time_constant_s` | s | OCV filter time constant while current is near zero. Set 0 for no OCV filtering. |
| `power_limit_trim_limit_nm` | Nm | Symmetric clamp on PID trim torque. |
| `power_limit_kp` | Nm/W | Proportional trim gain. |
| `power_limit_ki` | Nm/(W*s) | Integral trim gain. Default is 0 for first deployment. |
| `power_limit_kd` | Nm/(W/s) | Derivative trim gain. Default is 0 for first deployment. |
| `power_limit_torque_rpm[]` | RPM | Strictly increasing RPM breakpoints for the 1D table. |
| `power_limit_torque_nm[]` | Nm | Available torque at each RPM breakpoint. |

Firmware defaults:

| Parameter | Default |
| --- | ---: |
| `max_torque_nm` | 220 Nm |
| `pedal_exponent` | 1.6 |
| `power_limit_w` | 75,000 W |
| `current_limit_a` | 200 A |
| `hard_current_cut_a` | 240 A |
| `hard_power_cut_w` | 80,000 W |
| `ocv_cell_count` | 130 |
| `ocv_lpf_time_constant_s` | 1.0 s |
| `power_limit_trim_limit_nm` | 20 Nm |
| `power_limit_kp` | 0.002 Nm/W |
| `power_limit_ki` | 0 |
| `power_limit_kd` | 0 |

## Outputs And Faults

| Output | Meaning |
| --- | --- |
| `torque_lookup_output` | 1D power-limit table torque at current RPM before derates. |
| `torque_derated` | Table torque after current-derived power scaling and OCV derate. |
| `torque_power_limited` | Pedal-shaped final torque after PID trim. |
| `torque_cmd` | Final model torque request before PRNDL may force park torque to zero. |
| `derate_factor_cell_voltage` | OCV derate ratio from 0 to 1. |
| `power_limit_input_fault` | Required parameters or live inputs are invalid. |
| `current_safety_cut` | Live bus current exceeded `hard_current_cut_a`. |
| `power_safety_cut` | Live bus power exceeded `hard_power_cut_w`. |

Debug outputs:

| Debug field | Meaning |
| --- | --- |
| `ocv_estimate_v` | Current pack OCV estimate. |
| `active_power_limit_w` | Active target after current-derived power limiting. |
| `measured_power_w` | Raw live `Vbus * Ibus`. |
| `low_voltage_derate_pct` | OCV derate as percent. |
| `power_limit_feedforward_torque_nm` | Derated 1D table torque before PID trim. |
| `power_limit_feedback_p_nm` | P contribution. |
| `power_limit_feedback_i_nm` | I contribution. |
| `power_limit_feedback_d_nm` | D contribution. |
| `power_limit_feedback_torque_nm` | Total clamped PID trim. |
| `power_limit_available_torque_nm` | Available torque after PID trim, before pedal multiplication. |
| `pedal_shaped_pct` | Pedal request after exponent shaping. |

## Functions

| Function | Purpose |
| --- | --- |
| `torque_map_init()` | Clears OCV and PID state. |
| `torque_map_evaluate()` | Main control step. Validates, looks up table torque, applies derates, trims with PID, and outputs torque/faults. |
| `power_limit_torque_map_is_valid()` | Confirms RPM breakpoints increase and torque values are finite and safe. |
| `torque_map_params_are_valid()` | Confirms all limiter parameters are finite and internally consistent. |
| `power_limit_torque_at_rpm()` | Interpolates the 1D RPM/torque table. |
| `shape_pedal()` | Applies the pedal exponent safely. |
| `compute_low_voltage_derate()` | Converts OCV estimate to the low-voltage torque derate. |
| `lpf_alpha_from_tau()` / `lpf_step()` | OCV-only low-pass helpers. |
| `vcu_can_set_powertrain_inputs()` | Firmware CAN adapter for inverter voltage/current/speed and HVC pack voltage. |

## Firmware CAN Wiring

`vcu_can_set_powertrain_inputs()` fills the model inputs from CAN:

- Motor speed uses `Inverter Speed` when fresh, with `Inverter Status` as a
  fallback.
- Power tracking uses `Inverter Voltage.dc_bus_voltage` and
  `Inverter Current.dc_bus_current`.
- `inverter_power_valid` is true only when both inverter voltage and current
  packets are fresh.
- HVC `Battery Pack Status.pack_voltage` is registered and used only for OCV
  when fresh. If stale, inverter DC bus voltage is the OCV fallback.

## Edge-Case Behavior

| Case | Behavior |
| --- | --- |
| Missing inverter voltage/current/speed | Zero torque, `power_limit_input_fault = true`. |
| Invalid table order or torque values | Zero torque, `power_limit_input_fault = true`. |
| Zero or negative DC bus voltage | Zero torque, `power_limit_input_fault = true`. |
| RPM below table | Uses first table value. |
| RPM above table | Uses last table value. |
| Pedal below zero or above one | Clamped to `[0, 1]`. |
| Pedal exponent outside `[0.1, 5.0]` | Invalid parameter fault, zero torque. |
| Current target lower than power target | Table torque scales down by active power ratio. |
| Hard current or power exceeded | Zero torque for that step and safety fault set. |
| OCV unavailable | Uses inverter bus voltage as OCV fallback. |

## Tuning Guidance

1. Recalculate the 1D RPM/torque table if `power_limit_w` changes.
2. Start with `power_limit_ki = 0` and `power_limit_kd = 0`.
3. Confirm inverter power and speed freshness on-car before driving.
4. Confirm inverter current sign: positive must mean discharge.
5. Log `measured_power_w`, `active_power_limit_w`, `torque_derated`,
   `power_limit_available_torque_nm`, and `pedal_shaped_pct`.
6. Tune the 1D table first. It should land slightly under target at full pedal.
7. Increase `power_limit_kp` only enough to close the remaining power error.
8. Add `power_limit_ki` only if steady-state error remains after table and P
   tuning.
9. Add `power_limit_kd` only if rising power spikes need damping.

## Verification

Model tests cover:

- Pedal requesting a percentage of available torque.
- Pedal exponent shaping.
- 1D RPM/torque interpolation.
- Current-limit scaling of available torque.
- PID trim using live measured electrical power.
- Stale inverter input fail-closed behavior.
- Invalid table fail-closed behavior.
- Hard power/current cuts.
- OCV affecting only low-voltage derate.

The embedded VCU firmware target builds with the CAN wiring and calibration
table in place.
