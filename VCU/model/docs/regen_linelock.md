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
- `500 psi -> 60 Nm`
- Linear slope between and beyond those points
- In normal mode, final command is clipped by the dynamic pack/motor regen limit
  and by the absolute inverter safety cap
- In pressure-only test mode, final command is clipped only by the absolute
  inverter safety cap

The inverter command remains on the normal direction pin setting. Regen is sent
as a negative `torque_request`.

## Pressure-Only Test Mode

The current test build enables `pressure_only_test_mode`.

This mode is intentionally basic for bring-up. When enabled, the VCU ignores the
OCV gate, pack temperature gates, motor speed gate, CAN-validity gates, and
APPS/BSE fault gates inside the regen linelock component. The regen command is:

```text
regen_torque = -clamp(rear_pressure_based_torque, 0 Nm, absolute_regen_torque_cap_nm)
```

`out->linelock_enabled` is true whenever the pressure-based torque request is
positive. The normal global disable flag and measured DC bus current hard cut
are still active.

Set `.pressure_only_test_mode = false` to restore the full validation logic
described below.

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

The VCU estimates pack OCV as:

```text
estimated_ocv = inverter_dc_bus_voltage - measured_regen_current * pack_resistance
```

where measured regen current is `-inverter_dc_bus_current` by default because
regen current is expected to be negative on the inverter CAN signal.

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

## Availability Gates

Regen/linelock can only activate when all of these are true:

- `params.regen_linelock.disable == false`
- Rear pressure is above `hard_cut_reset_pressure_psi` (`100 psi` default)
- Battery Pack Status, inverter voltage, inverter current, and motor speed
  inputs are valid
- Min cell temperature is above `10 C`
- Max cell temperature is below `50 C`
- Motor speed is above `215.4 rpm`, equivalent to about `5 kph` with a
  `3.3:1` motor-to-wheel ratio and `16 in` tire diameter
- Estimated pack OCV has fallen below `520.11 V`
- No APPS/BSE brake fault is active
- No regen hard-current-cut latch is active

The OCV gate has hysteresis. Once regen becomes available below `520.11 V`, it
stays available until estimated OCV rises above `522.11 V` by default. This
prevents chatter around the pack-voltage knee.

The lower temperature bound protects against lithium plating during charge. The
`10 C` default is deliberately conservative for first validation; below this
temperature the rear brakes remain mechanical.

## Hard Current Cut

The hard safety cut uses measured inverter DC bus current, not the estimator.

Default:

```text
hard_cut_current = 45 A * (1 + 0.20) = 54 A
```

In normal mode, if rear pressure is above `100 psi` and measured regen current
exceeds `54 A`, the VCU immediately:

- sets torque command to `0 Nm`
- disables linelock
- latches `regen_linelock_current_hard_cut`

The latch clears automatically once rear pressure falls to or below `100 psi`.

In pressure-only test mode, the hard cut is armed whenever the pressure-based
regen torque request is positive.

## VCU/PDU Command

The VCU now exposes `out->linelock_enabled`.

Current CAN placeholder behavior:

- `out->linelock_enabled == true` sets bit `Temp Command 1` in `Switch Command`
  (`0x143`, VCU to PDU)
- `out->linelock_enabled == false` clears the switch command

This is intentionally isolated to the VCU side. The PDU firmware still needs to
map the final linelock command bit to the real output channel.

## Main Parameters

The test-one tuning block is in:

```text
VCU/firmware/Core/Inc/params/default_params.h
```

under:

```c
.regen_linelock = {
    .disable = false,
    .pressure_only_test_mode = true,
    .dc_bus_current_regen_is_negative = true,
    .rear_pressure_zero_torque_psi = 0.0f,
    .rear_pressure_reference_psi = 500.0f,
    .regen_torque_at_reference_pressure_nm = 60.0f,
    .absolute_regen_torque_cap_nm = 230.0f,
    .pack_current_limit_a = 45.0f,
    .hard_cut_margin_pct = 0.20f,
    .hard_cut_reset_pressure_psi = 100.0f,
    .pack_terminal_voltage_limit_v = 546.0f,
    .pack_resistance_ohm = 0.442f,
    .dynamic_voltage_reserve_v = 6.0f,
    .pack_ocv_enable_v = 520.11f,
    .pack_ocv_disable_hysteresis_v = 2.0f,
    .min_cell_temp_c = 10.0f,
    .max_cell_temp_c = 50.0f,
    .min_motor_speed_rpm = 215.4f,
}
```

Set `.disable = true` to fully disable regen/linelock control in code.
