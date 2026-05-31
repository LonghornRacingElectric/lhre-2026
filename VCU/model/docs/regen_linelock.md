# VCU Regen Linelock Control

This document describes the first-test VCU logic for rear regen braking with the
rear linelock valve. The intent is simple and conservative: either the rear axle
is braking mechanically, or the rear axle is braking through motor regen. This
does not blend rear hydraulic caliper pressure with regen torque.

## System Behavior

The linelock valve is treated as a rear hydraulic isolation valve, not a rear
caliper pressure holding device.

- Linelock disabled/open: rear master-cylinder pressure reaches the rear caliper
  and rear braking is mechanical.
- Linelock enabled/energized: rear master-cylinder pressure is blocked from the
  rear caliper and the VCU requests negative inverter torque for rear axle regen.
- The check valve from the rear caliper side back to the master cylinder is
  expected to release caliper-side pressure if master-cylinder pressure falls
  below caliper pressure.

The VCU never intentionally holds rear caliper pressure during regen operation.

## Driver Brake Demand

Regen demand comes from the rear BSE/pre-lock pressure, `out->bse2_psi`.

Default mapping:

- `0 psi -> 0 Nm`
- `500 psi -> 76 Nm`
- Linear slope between and beyond those points
- In normal mode, final command is clipped by the dynamic pack/motor regen limit
  and by the absolute inverter safety cap
- In pressure-only test mode, final command is clipped only by the absolute
  inverter safety cap

The inverter command remains on the normal direction pin setting. Regen is sent
as a negative `torque_request`.

## Pressure-Only Test Mode

The current test build disables `pressure_only_test_mode`.

This mode is intentionally basic for bring-up. When enabled, the VCU ignores the
OCV gate, motor speed gate, inverter CAN-validity gates, and APPS fault gate
inside the regen linelock component. The regen command is:

```text
regen_torque = -clamp(rear_pressure_based_torque, 0 Nm, absolute_regen_torque_cap_nm)
```

`out->linelock_enabled` is true whenever the pressure-based torque request is
positive, but negative torque is held at `0 Nm` until the linelock command has
been continuously active for `linelock_close_delay_ms`. The normal global
disable flag and measured DC bus current hard cut are still active.

Set `.pressure_only_test_mode = true` only for controlled bring-up when the
normal validation logic below needs to be bypassed temporarily.

## Pack Regen Limit

The pack regen limit is copied from the HTML regen torque tool, but the VCU uses
total pack current directly. The default current limit is:

- `45 A` total pack current
- Equivalent to `9 A/cell` for the `5p` pack

Default pack parameters:

- Pack terminal voltage limit: `546 V`
- Dynamic voltage reserve: `6 V`
- Pack resistance: `0.442 ohm`
- Current limit: `45 A`

The resistance value uses the Molicel P30B datasheet AC impedance value of
`17 mohm` per cell:

```text
130 series * 0.017 ohm / 5 parallel = 0.442 ohm
```

The default OCV knee is:

```text
546 V - 6 V - 45 A * 0.442 ohm = 520.11 V
```

The VCU estimates pack OCV from the max cell voltage seen at low current:

```text
max_cell_ocv = filtered max_cell_voltage when -1 A < dc_bus_current < 1 A
estimated_pack_ocv = max_cell_ocv * 130 series cells
```

This matches the existing min-cell OCV estimator used by the battery model, but
uses the max cell because regen over-voltage risk is set by the highest cell.

Available pack current is:

```text
headroom = terminal_voltage_limit - estimated_ocv - dynamic_voltage_reserve
available_current = clamp(headroom / pack_resistance, 0 A, pack_current_limit)
```

Available motor regen torque is:

```text
omega = abs(motor_rpm) * 2*pi/60
terminal_voltage = min(estimated_ocv + available_current * pack_resistance,
                       terminal_voltage_limit)
torque_limit = terminal_voltage * available_current / omega
```

Motor RPM is used directly as the speed source.

The torque/current limit now uses the larger of:

- filtered max-cell OCV estimate
- live max cell voltage multiplied by `130`

This prevents a stale low-current OCV estimate from allowing too much regen when
the live max cell voltage has risen.

## Availability Gates

Regen/linelock can only activate when all of these are true:

- `params.regen_linelock.disable == false`
- Rear pressure is at least `10 psi`
- Pre-regen pedal torque request is at or below `20 Nm`. Above `20 Nm`, the VCU
  keeps linelock open and does not allow regen so the valve can cool and any rear
  caliper-side pressure can release mechanically.
- The linelock command has been continuously true for `500 ms`. Before that
  delay expires, the VCU may command the linelock closed but keeps regen torque
  at `0 Nm`. This delay is intentionally longer than the current PDU linelock
  loop in `ad/pdu-linelock`, which polls `VCU State.line_lock_enabled` and
  updates the GPIO every `200 ms`.
- Inverter current and motor speed inputs are valid
- Motor speed is above `219.49 rpm`, equivalent to about `5 kph` with a
  `43:13` motor-to-wheel ratio and `7.87 in` loaded tire radius
- Estimated pack OCV has fallen below `520.11 V`
- Live max cell voltage is below `4.05 V`. This is a separate hard block from
  the filtered OCV latch. A live max cell voltage of `4.093 V` blocks regen.
- No APPS fault is active
- No regen hard-current-cut latch is active

The OCV gate has hysteresis. Once regen becomes available below `520.11 V`, it
stays available until estimated OCV rises above `522.11 V` by default. This
prevents chatter around the pack-voltage knee.

Pack temperature is not currently used as a regen cut condition because the
available `Battery Pack Status` packet is not fully implemented. The parameter
default remains `55 C` for the future high-temperature cutoff, but that cutoff
must be connected to a verified cell-temperature source before it is used.

## Hard Current Cut

The hard safety cut uses measured inverter DC bus current, not the estimator.

Default:

```text
hard_cut_current = 45 A * (1 + 0.20) = 54 A
```

If a pressure-based regen torque request is positive and measured regen current
exceeds `54 A`, the VCU immediately:

- sets torque command to `0 Nm`
- disables linelock
- latches `regen_linelock_current_hard_cut`

The latch clears automatically once rear pressure falls to or below `100 psi`.

The hard cut is armed whenever the pressure-based regen torque request is
positive.

## Linelock/Regen Coupling

The VCU has two software protections against commanding regen while its own
linelock command is open:

- Regen torque is delayed until `out->linelock_enabled` has been true
  continuously for `linelock_close_delay_ms`.
- A final model invariant forces negative `torque_cmd` back to `0 Nm` and sets
  `regen_linelock_command_mismatch` if any later code path ever leaves
  `torque_cmd < 0` while `out->linelock_enabled == false`.

The current `ad/pdu-linelock` PDU branch does not publish a linelock feedback or
acknowledge bit. With that PDU code, the VCU cannot prove the physical valve is
closed; it can only request `VCU State.line_lock_enabled`, wait longer than the
PDU control loop, and then allow regen. A true physical guarantee requires a PDU
feedback signal from the actual linelock output voltage/current or a validated
PDU-side active acknowledge bit.

## CAN Inputs

The regen/linelock checks intentionally avoid HVC, PDU, and VCU CAN timeout
gates. The only CAN timeout gates used for cutting regen are inverter feedback
timeouts:

- `Inverter Current` (`0x0A6`, 100 Hz, 200 ms timeout): DC bus current for the
  regen current hard cut and low-current OCV sampling.
- `Inverter Speed` (`0x0B0`) or `Inverter Status` (`0x0A5`): motor speed for
  the low-speed cutoff. The VCU accepts either packet as the speed source.

Other inputs are values only, not timeout gates:

- `Battery Cell Limits` (`0x136`) provides max cell voltage for the max-cell OCV
  estimator, but its CAN timeout is not used to cut regen.
- `Battery Pack Status` (`0x132`) is not used by regen/linelock because it is
  not fully implemented.

## APPS/BSE Faults

The regen/linelock component respects APPS faults. The top-level VCU model also
zeros torque and opens linelock on APPS fault or regen hard-current-cut fault.

- APPS faults are active: sensor mismatch over `0.15` for more than `100 ms`,
  APPS travel over `1.0`, or APPS travel under `-0.5`.
- BSE voltage range faults are currently disabled in code.
- The brake/accelerator latch still exists in the BSE component for telemetry,
  but regen/linelock does not use `brake_any_fault` as a cut condition. This
  keeps rear brake pressure available for regen demand while positive drive
  torque during a regen pressure request is cut separately.

## VCU Command

The VCU now exposes `out->linelock_enabled`.

Current CAN behavior:

- VCU sends `out->linelock_enabled` as `Line Lock Enabled` in `VCU State`
  (`0x1C7`, VCU to Pi/PDU).
- The previous `Switch Command` / `Temp Command 1` placeholder is no longer sent
  by the VCU for linelock.
- PDU output handling is intentionally not included in this branch. The PDU
  firmware should consume `VCU State.line_lock_enabled` and drive the real
  linelock output in its own branch/change.

## Main Parameters

The test-one tuning block is in:

```text
VCU/firmware/Core/Inc/params/default_params.h
```

under:

```c
.regen_linelock = {
    .disable = false,
    .pressure_only_test_mode = false,
    .dc_bus_current_regen_is_negative = true,
    .rear_pressure_zero_torque_psi = 0.0f,
    .rear_pressure_reference_psi = 500.0f,
    .rear_pressure_min_engage_psi = 10.0f,
    .regen_torque_at_reference_pressure_nm = 76.0f,
    .absolute_regen_torque_cap_nm = 230.0f,
    .pedal_torque_release_threshold_nm = 20.0f,
    .linelock_close_delay_ms = 500u,
    .pack_current_limit_a = 45.0f,
    .hard_cut_margin_pct = 0.20f,
    .hard_cut_reset_pressure_psi = 100.0f,
    .pack_terminal_voltage_limit_v = 546.0f,
    .pack_resistance_ohm = 0.442f,
    .pack_series_cell_count = 130.0f,
    .dynamic_voltage_reserve_v = 6.0f,
    .pack_ocv_enable_v = 520.11f,
    .pack_ocv_disable_hysteresis_v = 2.0f,
    .max_cell_voltage_regen_disable_v = 4.05f,
    .min_cell_temp_c = 10.0f,
    .max_cell_temp_c = 55.0f,
    .min_motor_speed_rpm = 219.49f,
}
```

Set `.disable = true` to fully disable regen/linelock control in code.
