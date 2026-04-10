from dataclasses import dataclass, field
from typing import List, Optional

@dataclass
class OrionDynamics:
    gps: List[float] = field(default_factory=list)
    gps_imu: List[float] = field(default_factory=list)
    accel_pedal_travel: Optional[float] = None
    steer_col_angle: Optional[float] = None
    bl_sprung_accel: List[float] = field(default_factory=list)
    bl_unsprung_accel: List[float] = field(default_factory=list)
    br_sprung_accel: List[float] = field(default_factory=list)
    br_unsprung_accel: List[float] = field(default_factory=list)
    fl_sprung_accel: List[float] = field(default_factory=list)
    fl_unsprung_accel: List[float] = field(default_factory=list)
    fr_sprung_accel: List[float] = field(default_factory=list)
    fr_unsprung_accel: List[float] = field(default_factory=list)
    bl_ride_height: Optional[float] = None
    bl_strain_gauge_v: Optional[float] = None
    bl_sus_pot_v: Optional[float] = None
    blw_speed: Optional[float] = None
    br_ride_height: Optional[float] = None
    br_strain_gauge_v: Optional[float] = None
    br_sus_pot_v: Optional[float] = None
    brw_speed: Optional[float] = None
    fl_ride_height: Optional[float] = None
    fl_strain_gauge_v: Optional[float] = None
    fl_sus_pot_v: Optional[float] = None
    flw_speed: Optional[float] = None
    fr_ride_height: Optional[float] = None
    fr_strain_gauge_v: Optional[float] = None
    fr_sus_pot_v: Optional[float] = None
    frw_speed: Optional[float] = None
    ride_height: Optional[float] = None
    wheel_speed: Optional[float] = None

@dataclass
class OrionControls:
    motor_speed: Optional[float] = None
    torque_feedback: Optional[float] = None
    apps1_travel: Optional[float] = None
    apps1_v: Optional[float] = None
    apps2_travel: Optional[float] = None
    apps2_v: Optional[float] = None
    bpps1_travel: Optional[float] = None
    bpps1_v: Optional[float] = None
    bpps2_travel: Optional[float] = None
    bpps2_v: Optional[float] = None
    brake_bias: Optional[float] = None
    brake_light_pct: Optional[float] = None
    brake_pressure_f: Optional[float] = None
    brake_pressure_rall: Optional[float] = None
    brake_pressure_rbll: Optional[float] = None
    bse1_v: Optional[float] = None
    bse2_v: Optional[float] = None
    bse3_v: Optional[float] = None
    lights_current: Optional[float] = None
    rpm_request: Optional[float] = None
    torque_command: Optional[float] = None
    torque_limit: Optional[float] = None
    torque_request: Optional[float] = None
    commanded_torque: Optional[float] = None
    motor_angle: Optional[float] = None
    direction: Optional[bool] = None
    enable: Optional[bool] = None
    torque_shudder: Optional[float] = None

@dataclass
class OrionPack:
    bus_voltage: Optional[float] = None
    lv_boards_current: Optional[float] = None
    cells_v: List[float] = field(default_factory=list)
    dc_bus_v: Optional[float] = None
    delta_resolver_angle: Optional[float] = None
    inverter_freq: Optional[float] = None
    neutral_output_v: Optional[float] = None
    time_since_on: Optional[float] = None
    vab_vq_v: Optional[float] = None
    vbc_vd_v: Optional[float] = None
    cells_temps: List[float] = field(default_factory=list)
    dc_bus_current: Optional[float] = None
    hv_c: Optional[float] = None
    hv_pack_v: Optional[float] = None
    hv_soc: Optional[float] = None
    lv_batt_c: Optional[float] = None
    lv_batt_t: Optional[float] = None
    lv_batt_v: Optional[float] = None
    phase_a_current: Optional[float] = None
    phase_b_current: Optional[float] = None
    phase_c_current: Optional[float] = None

@dataclass
class OrionDiagnosticsHigh:
    shutdown_current: Optional[float] = None
    hvc_state_machine: Optional[float] = None
    post_faults: Optional[float] = None
    run_faults: Optional[float] = None
    neg_hv_contactor: Optional[bool] = None
    pos_hv_contactor: Optional[bool] = None
    precharge_contactor: Optional[bool] = None

@dataclass
class OrionDiagnosticsLow:
    precharge_r_temp: Optional[float] = None
    bmb_comm_error: Optional[bool] = None
    imd_gnd_isolation_error: Optional[bool] = None
    r2d_authorized: Optional[bool] = None
    r2d_status: Optional[bool] = None
    shutdown_leg1: Optional[bool] = None
    shutdown_leg2: Optional[bool] = None
    shutdown_leg3: Optional[bool] = None
    shutdown_leg4: Optional[bool] = None

@dataclass
class OrionThermal:
    batt_cooling_current: Optional[float] = None
    motor_cooling_current: Optional[float] = None
    motor_temp: Optional[float] = None
    ambient_temp: Optional[float] = None
    batt_loop_batt_temp: Optional[float] = None
    batt_loop_rad_fan_speed: Optional[float] = None
    batt_loop_rad_temp: Optional[float] = None
    bus_bar_temp1: Optional[float] = None
    bus_bar_temp2: Optional[float] = None
    bus_bar_temp3: Optional[float] = None
    cell_bottom_temp: Optional[float] = None
    cell_top_temp: Optional[float] = None
    coolant_temp: Optional[float] = None
    discharge_r_temp: Optional[float] = None
    gate_driver_temp: Optional[float] = None
    inverter_hotspot_temp: Optional[float] = None
    inverter_temp: Optional[float] = None
    module_a_temp: Optional[float] = None
    module_b_temp: Optional[float] = None
    module_c_temp: Optional[float] = None
    motor_loop_inverter_temp: Optional[float] = None
    motor_loop_motor_temp: Optional[float] = None
    motor_loop_rad_fan_speed: Optional[float] = None
    motor_loop_rad_temp: Optional[float] = None

@dataclass
class OrionSensorData:
    packet_id: int
    time: int
    dynamics: Optional[OrionDynamics] = None
    controls: Optional[OrionControls] = None
    pack: Optional[OrionPack] = None
    diagnostics_high: Optional[OrionDiagnosticsHigh] = None
    diagnostics_low: Optional[OrionDiagnosticsLow] = None
    thermal: Optional[OrionThermal] = None