# Grafana Dashboard Proposals — Orion Drive Day

This document tracks proposed Grafana dashboards for drive-day use. Each entry lists the target folder
(`real-time/` or `retrospect/`), the signals it draws from, and the rationale.

For provisioning conventions see the existing dashboards under
`telemtry/analysis/database/dashboards/orion/`.

---

## Status

| Dashboard | Folder | Status |
|---|---|---|
| Fault & Shutdown Status | real-time | Done — `real-time/fault_status.json` |
| Thermal Headroom | real-time | Done — `real-time/thermal_headroom.json` |
| Driver Inputs Timeline | retrospect | Done — `retrospect/driver_inputs.json` |
| Power & Energy Efficiency | retrospect | Done — `retrospect/power_energy.json` |
| Chassis & Suspension Analysis | retrospect | Done — `retrospect/chassis_suspension.json` |
| Driver Style / GG Expanded | retrospect | Done — `retrospect/driver_style.json` |

---

## 1. Fault & Shutdown Status *(real-time)*

**Rationale**: The highest-priority safety gap. No single dashboard shows the full shutdown circuit,
sensor plausibility, fuse bank, and HVC state together. This goes on a dedicated monitor at the pit wall.

### Panels

#### Shutdown Circuit (stat panels, boolean — green=closed/OK, red=open/fault)
- `shutdown_leg1` – `shutdown_leg4` (`diagnostics_low`)
- `neg_hv_contactor`, `pos_hv_contactor`, `precharge_contactor` (`diagnostics_high`)
- `r2d_status`, `r2d_authorized` (`diagnostics_low`)
- `shutdown_current` (`diagnostics_high`) — numeric, flag if < expected threshold

#### Sensor Plausibility (stat panels, boolean)
- `apps_implause`, `apps_mismatch` (`diagnostics_high`)
- `bpps_mismatch` (`diagnostics_high`)
- `apps1_disconnect`, `apps2_disconnect` (`diagnostics_high`)
- `bpps1_disconnect`, `bpps2_disconnect` (`diagnostics_high`)
- `bse1_disconnect`, `bse2_disconnect` (`diagnostics_high`)
- `imd_gnd_isolation_error`, `bmb_comm_error` (`diagnostics_low`)

#### Fuse Bank (stat panels, boolean — green=OK, red=blown)
- `boards_fuse`, `rtd_fuse`, `shtdn_fuse`, `ll_fuse` (`diagnostics_high`)
- `motor_pump_fuse`, `batt_pump_fuse`, `batt_fans_fuse` (`diagnostics_high`)
- `brake_light_fuse`, `spare_fuse` (`diagnostics_high`)

#### HVC / Controller State (mixed)
- `hvc_state_machine` — stat with named value mappings (0=Init, 1=Precharge, 2=Ready, 5=Running, etc.)
- `post_faults`, `run_faults` — numeric stat, red if non-zero
- `prndl_state` — stat with named value mappings
- `stomp_fault` — boolean stat

---

## 2. Thermal Headroom *(real-time)*

**Rationale**: Existing thermals dashboards show raw values. Trackside engineers need to see
*proximity to limit*, not just temperature. Color-coded gauges give instant "how close to pulling
the car" visibility.

### Limits reference (update if electronics team changes limits)
| Node | Yellow | Red |
|---|---|---|
| Inverter hotspot | 75°C | 85°C |
| Gate driver | 80°C | 90°C |
| Motor | 100°C | 120°C |
| Cell top/bottom | 40°C | 50°C |
| Battery loop | 40°C | 50°C |
| Coolant | 40°C | 50°C |

### Panels

#### Inverter Thermal (gauge panels)
- `inverter_hotspot_temp` — gauge, 0–100°C, thresholds at 75/85
- `gate_driver_temp` — gauge
- `module_a_temp`, `module_b_temp`, `module_c_temp` — gauge
- `inverter_temp` — gauge

#### Motor & Cooling (gauge panels)
- `motor_temp` — gauge, 0–130°C
- `motor_loop_motor_temp` — gauge
- `coolant_temp` — gauge
- `coolant_flow_lpm` — gauge, 0–20 L/min, red if < 2 (pump/flow issue)

#### Battery (gauge panels)
- `cell_top_temp` — gauge, 0–55°C
- `cell_bottom_temp` — gauge
- `batt_loop_batt_temp` — gauge
- `batt_loop_rad_temp` — gauge

#### LV / Precharge Resistors (stat panels)
- `precharge_r_temp` — stat, red above 80°C
- `discharge_r_temp` — stat, red above 80°C
- `lv_batt_t` — stat

---

## 3. Driver Inputs Timeline *(real-time + retrospect)*

**Rationale**: Existing controls dashboards split signals across multiple panels. A single scrollable
timeline lets engineers coach the driver in real time — "you're braking too early into T3" is obvious
when throttle, brake, speed, and torque are all on the same x-axis.

### Panels

#### Pedal Inputs (timeseries)
- `accel_pedal_travel` (%) — primary series
- `brake_pressure_f` (psi) — overlaid on secondary y-axis
- Annotation: overlap region (both > 5%) highlighted — APPS/BPPS conflict indicator

#### Steering & Speed (timeseries)
- `steer_col_angle` (deg)
- `gps_speed` (m/s) — secondary y-axis

#### Torque Delivery (timeseries)
- `torque_request`
- `torque_command`
- `torque_feedback`
- `torque_limit` — displayed as a ceiling line; any clipping shows the controller is capping demand

#### Wheel Speed Balance (timeseries, for lock-up detection)
- `flw_speed`, `frw_speed`, `blw_speed`, `brw_speed` — divergence on braking = lock-up

---

## 4. Power & Energy Efficiency *(retrospect)*

**Rationale**: Endurance dashboard only shows SoC + battery temperature. Engineers need to see
instantaneous power, regen capture, and depletion rate to make strategy calls and diagnose
abnormal drain.

### Computed signals (SQL-derived)
- `power_kw` = `hv_pack_v * hv_c / 1000.0` — positive = discharge, negative = regen
- `soc_rate` = `d(hv_soc)/dt` — approximate via `LAG()` window

### Panels

#### Instantaneous Power (timeseries)
- `hv_pack_v * hv_c / 1000.0` as `power_kw`
- Threshold band: positive (discharge, orange), negative (regen, blue)

#### SoC Over Session (timeseries)
- `hv_soc` — primary series
- `hv_pack_v` — secondary y-axis
- `hv_c` — tertiary (current)

#### Energy Balance (stat panels)
- Total energy discharged (J) = `SUM(hv_pack_v * hv_c * dt)` where `hv_c > 0`
- Total energy recovered (J) = `SUM(ABS(hv_pack_v * hv_c * dt))` where `hv_c < 0`
- Regen ratio = recovered / discharged (%)

#### Phase Currents (timeseries — for inverter efficiency)
- `phase_a_current`, `phase_b_current`, `phase_c_current`
- `dc_bus_current`

---

## 5. Chassis & Suspension Analysis *(retrospect)*

**Rationale**: The retrospect dynamics dashboard shows wheel speeds, ride heights, and strain gauges
separately. These cross-corner comparisons reveal setup issues (unbalanced springs, stuck damper,
aero imbalance) that are invisible when looking at signals in isolation.

### Panels

#### Ride Height — All 4 Corners (timeseries)
- `fl_ride_height`, `fr_ride_height`, `bl_ride_height`, `br_ride_height` — all on one plot
- Pitch visible as front-rear divergence; roll visible as left-right divergence

#### Corner Load Balance (timeseries)
- `fl_strain_gauge_v`, `fr_strain_gauge_v`, `bl_strain_gauge_v`, `br_strain_gauge_v`
- Delta plots: `fl - fr`, `bl - br` (lateral balance), `fl+fr - bl+br` (front-rear balance)

#### Suspension Pot Velocities (timeseries — computed)
- `d(fl_sus_pot_v)/dt`, `d(fr_sus_pot_v)/dt`, `d(bl_sus_pot_v)/dt`, `d(br_sus_pot_v)/dt`
- Approximated in SQL via `LAG()`. High amplitude = corner hitting bump stops or damper issue.

#### Wheel Speed Differentials (timeseries — slip indicator)
- `flw_speed - blw_speed` (left side slip)
- `frw_speed - brw_speed` (right side slip)
- `(flw_speed + frw_speed)/2 - (blw_speed + brw_speed)/2` (front-rear slip ratio proxy)

#### Accelerometer Comparison (timeseries)
- `bl_sprung_accel[z]`, `br_sprung_accel[z]`, `fl_sprung_accel[z]`, `fr_sprung_accel[z]`
  vs unsprung counterparts — large delta = high damper activity at that corner

---

## 6. Driver Style / GG Expanded *(retrospect)*

**Rationale**: The existing `gps.json` GG plot is a plain X-Y scatter. Adding color encoding and
derived views makes it a proper driver coaching tool — a driver's GG "signature" is immediately
visible.

### Panels

#### GG Colored by Speed (xychart / scatter)
- X: `gps_imu[1]` (lateral accel, m/s²)
- Y: `gps_imu[0]` (longitudinal accel, m/s²)
- Color: `gps_speed` — low speed dots near origin, high speed dots at the limit circle
- Interpretation: driver using full friction circle at speed vs lifting early

#### GG Colored by Throttle (xychart / scatter)
- X: lateral accel, Y: longitudinal accel
- Color: `accel_pedal_travel`
- Shows the rotation point: where does the driver go back to throttle mid-corner?

#### Throttle-Speed Profile (xychart / scatter)
- X: `gps_speed` (m/s), Y: `accel_pedal_travel` (%)
- Shows whether driver is at WOT at speed, or modulating. Flat top = pushing hard.

#### Brake Consistency (xychart / scatter)
- X: `brake_pressure_f` (psi), Y: `gps_imu[0]` (longitudinal decel, m/s²)
- Tight cluster = consistent braking. Scattered = inconsistent pedal application.

#### Steering vs Lateral Accel (xychart / scatter)
- X: `steer_col_angle` (deg), Y: `gps_imu[1]` (lateral accel)
- Linear at low speed; nonlinear at limit = understeer/oversteer signature

---

## Implementation Notes

- All SQL queries use `rawQuery: true` with `format: table` per existing dashboard conventions.
- Repeated array fields (`gps_imu`, `bl_sprung_accel`, etc.) are indexed via
  `(column)[index]` in PostgreSQL array syntax (1-based).
- For computed signals using `LAG()` or `d/dt`, wrap in a CTE for readability.
- Time column must be returned as `"time"` with `time AS "time"` alias for Grafana time-series panels.
- Dashboard `uid` values must be globally unique; use a descriptive slug.
- All new dashboards go in `telemtry/analysis/database/dashboards/orion/real-time/` or `retrospect/`
  and are auto-provisioned by the Grafana container at startup.
