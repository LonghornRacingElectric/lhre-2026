from dataclasses import dataclass, field
from typing import List, Optional

@dataclass
class OrionDynamics:
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

@dataclass
class OrionControls:
    apps1_travel: Optional[float] = None
    apps1_v: Optional[float] = None
    apps2_travel: Optional[float] = None
    apps2_v: Optional[float] = None
    bpps1_travel: Optional[float] = None
    bpps1_v: Optional[float] = None
    bpps2_travel: Optional[float] = None
    bpps2_v: Optional[float] = None
    brake_bias: Optional[float] = None
    brake_pressure_f: Optional[float] = None
    brake_pressure_rall: Optional[float] = None
    brake_pressure_rbll: Optional[float] = None
    bse1_v: Optional[float] = None
    bse2_v: Optional[float] = None
    bse3_v: Optional[float] = None
    rpm_request: Optional[float] = None
    torque_request: Optional[float] = None

@dataclass
class OrionPack:
    torque_command: Optional[float] = None
    dc_bus_v: Optional[float] = None
    hv_c: Optional[float] = None
    hv_pack_v: Optional[float] = None
    hv_soc: Optional[float] = None
    lv_batt_c: Optional[float] = None
    lv_batt_t: Optional[float] = None
    lv_batt_v: Optional[float] = None

@dataclass
class OrionDiagnosticsLow:
    precharge_r_temp: Optional[float] = None
    bmb_comm_error: Optional[bool] = None
    imd_gnd_isolation_error: Optional[bool] = None
    shutdown_leg1: Optional[bool] = None
    shutdown_leg2: Optional[bool] = None
    shutdown_leg3: Optional[bool] = None
    shutdown_leg4: Optional[bool] = None

@dataclass
class OrionThermal:
    cells_v: List[float] = field(default_factory=list)
    cells_temps: List[float] = field(default_factory=list)
    ambient_temp: Optional[float] = None
    batt_loop_batt_temp: Optional[float] = None
    batt_loop_rad_fan_speed: Optional[float] = None
    batt_loop_rad_temp: Optional[float] = None
    bus_bar_temp1: Optional[float] = None
    bus_bar_temp2: Optional[float] = None
    bus_bar_temp3: Optional[float] = None
    discharge_r_temp: Optional[float] = None
    inverter_temp: Optional[float] = None
    motor_loop_inverter_temp: Optional[float] = None
    motor_loop_motor_temp: Optional[float] = None
    motor_loop_rad_fan_speed: Optional[float] = None
    motor_loop_rad_temp: Optional[float] = None
    motor_temp: Optional[float] = None

@dataclass
class OrionSensorData:
    packet_id: int
    time: int
    dynamics: Optional[OrionDynamics] = None
    controls: Optional[OrionControls] = None
    pack: Optional[OrionPack] = None
    diagnostics_low: Optional[OrionDiagnosticsLow] = None
    thermal: Optional[OrionThermal] = None