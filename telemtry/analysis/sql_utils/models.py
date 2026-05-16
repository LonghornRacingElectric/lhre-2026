from sqlalchemy import (
    create_engine,
    Column,
    Integer,
    String,
    Float,
    Boolean,
    BigInteger,
    Date,
    SmallInteger,
    Text,
    ForeignKey,
)
from sqlalchemy.dialects.postgresql import ARRAY, BYTEA, JSONB
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship
from sqlalchemy import MetaData

Base = declarative_base()

# Separate bases to reflect two distinct DB schemas
BaseTelemetry = declarative_base(metadata=MetaData())  # Nightwatch / telemetry DB
BaseAngelique = declarative_base(metadata=MetaData())   # Angelique DB
BaseOrion = declarative_base(metadata=MetaData())       # Orion DB

# -----------------------------
# Shared Base models (as before)
# -----------------------------

class DriveDay(Base):
    __tablename__ = 'drive_day'
    day_id = Column(SmallInteger, primary_key=True, autoincrement=True)
    date = Column(Date, nullable=False)
    power_limit = Column(Integer)
    events = relationship("Event", back_populates="drive_day", overlaps="drive_day")

class LutDriver(Base):
    __tablename__ = 'lut_driver'
    driver_id = Column(SmallInteger, primary_key=True)
    driver_name = Column(Text, nullable=False)
    driver_weight = Column(SmallInteger)
    events = relationship("Event", back_populates="driver")

class LutLocation(Base):
    __tablename__ = 'lut_location'
    location_id = Column(SmallInteger, primary_key=True)
    area = Column(Text, nullable=False)
    track = Column(Text, nullable=False)
    events = relationship("Event", back_populates="location")

class LutCar(Base):
    __tablename__ = 'lut_car'
    car_id = Column(SmallInteger, primary_key=True)
    car_name = Column(Text, nullable=False)
    events = relationship("Event", back_populates="car")

class LutEventType(Base):
    __tablename__ = 'lut_event_type'
    type_id = Column(SmallInteger, primary_key=True)
    event_type = Column(Text, nullable=False)
    events = relationship("Event", back_populates="event_type_ref")

class Classifier(Base):
    __tablename__ = 'classifier'
    event_id = Column(BigInteger, ForeignKey('event.event_id'), primary_key=True)
    type = Column(Text, nullable=False, primary_key=True)
    start_time = Column(BigInteger, nullable=False, primary_key=True)
    end_time = Column(BigInteger)
    notes = Column(Text)
    event = relationship("Event", back_populates="classifiers", overlaps="classifiers,event")

class Event(Base):
    __tablename__ = 'event'
    event_id = Column(SmallInteger, primary_key=True, autoincrement=True)
    day_id = Column(SmallInteger, ForeignKey('drive_day.day_id'), nullable=False)
    status = Column(SmallInteger)
    creation_time = Column(BigInteger, nullable=False)
    start_time = Column(BigInteger)
    end_time = Column(BigInteger)
    packet_start = Column(BigInteger)
    packet_end = Column(BigInteger)
    car_id = Column(SmallInteger, ForeignKey('lut_car.car_id'), nullable=False)
    driver_id = Column(SmallInteger, ForeignKey('lut_driver.driver_id'), nullable=False)
    location_id = Column(SmallInteger, ForeignKey('lut_location.location_id'), nullable=False)
    event_type = Column(SmallInteger, ForeignKey('lut_event_type.type_id'), nullable=False)
    event_index = Column(SmallInteger)
    car_weight = Column(SmallInteger)
    tow_angle = Column(Float)
    # Alignment per axle
    camber_front = Column(Float)
    camber_rear = Column(Float)
    toe_front = Column(Float)
    toe_rear = Column(Float)
    # Ride height per axle (+ legacy single)
    ride_height_front = Column(Float)
    ride_height_rear = Column(Float)
    ride_height = Column(Float)
    ackerman_adjustment = Column(Float)
    shock_dampening = Column(SmallInteger)
    power_limit = Column(Integer)
    torque_limit = Column(SmallInteger)
    frw_pressure = Column(Float)
    flw_pressure = Column(Float)
    brw_pressure = Column(Float)
    blw_pressure = Column(Float)
    # Tire wear depth and durometer
    fr_wear_depth = Column(Float)
    fl_wear_depth = Column(Float)
    rr_wear_depth = Column(Float)
    rl_wear_depth = Column(Float)
    fr_durometer = Column(Float)
    fl_durometer = Column(Float)
    rr_durometer = Column(Float)
    rl_durometer = Column(Float)
    # Damping per corner (LSC/LSR/HSC/HSR)
    fr_lsc = Column(SmallInteger)
    fr_lsr = Column(SmallInteger)
    fr_hsc = Column(SmallInteger)
    fr_hsr = Column(SmallInteger)
    fl_lsc = Column(SmallInteger)
    fl_lsr = Column(SmallInteger)
    fl_hsc = Column(SmallInteger)
    fl_hsr = Column(SmallInteger)
    rr_lsc = Column(SmallInteger)
    rr_lsr = Column(SmallInteger)
    rr_hsc = Column(SmallInteger)
    rr_hsr = Column(SmallInteger)
    rl_lsc = Column(SmallInteger)
    rl_lsr = Column(SmallInteger)
    rl_hsc = Column(SmallInteger)
    rl_hsr = Column(SmallInteger)
    front_wing_on = Column(Boolean)
    rear_wing_on = Column(Boolean)
    front_wing_pitch = Column(Float)
    rear_wing_pitch = Column(Float)
    regen_on = Column(Boolean)
    undertray_on = Column(Boolean)
    # 2026 per-axle corner spring rates (Nightwatch)
    front_corner_spring_rate = Column(Float)
    rear_corner_spring_rate = Column(Float)
    # ARB settings (free text)
    front_arb_setting = Column(Text)
    rear_arb_setting = Column(Text)
    drive_day = relationship("DriveDay", back_populates="events")
    car = relationship("LutCar", back_populates="events")
    driver = relationship("LutDriver", back_populates="events")
    location = relationship("LutLocation", back_populates="events")
    event_type_ref = relationship("LutEventType", back_populates="events")
    classifiers = relationship("Classifier", back_populates="event")

class Packet(BaseTelemetry):
    __tablename__ = 'packet'
    packet_id = Column(BigInteger, primary_key=True)
    time = Column(BigInteger, nullable=False)
    dynamics = relationship("Dynamics", uselist=False, back_populates="packet")
    controls = relationship("Controls", uselist=False, back_populates="packet")
    pack = relationship("Pack", uselist=False, back_populates="packet")
    diagnostics_high = relationship("DiagnosticsHigh", uselist=False, back_populates="packet")
    diagnostics_low = relationship("DiagnosticsLow", uselist=False, back_populates="packet")
    thermal = relationship("Thermal", uselist=False, back_populates="packet")

class Dynamics(BaseTelemetry):
    __tablename__ = 'dynamics'
    packet_id = Column(BigInteger, ForeignKey('packet.packet_id'), primary_key=True)
    steer_col_angle = Column(Float)
    fl_steer_angle = Column(Float)
    fr_steer_angle = Column(Float)
    fl_sprung_accel = Column(ARRAY(Float))
    fr_sprung_accel = Column(ARRAY(Float))
    bl_sprung_accel = Column(ARRAY(Float))
    br_sprung_accel = Column(ARRAY(Float))
    fl_unsprung_accel = Column(ARRAY(Float))
    fr_unsprung_accel = Column(ARRAY(Float))
    bl_unsprung_accel = Column(ARRAY(Float))
    br_unsprung_accel = Column(ARRAY(Float))
    fl_sprung_ang_rate = Column(ARRAY(Float))
    fr_sprung_ang_rate = Column(ARRAY(Float))
    bl_sprung_ang_rate = Column(ARRAY(Float))
    br_sprung_ang_rate = Column(ARRAY(Float))
    cent_mass_accel = Column(ARRAY(Float))
    cent_mass_ang_rate = Column(ARRAY(Float))
    flw_speed = Column(Float)
    frw_speed = Column(Float)
    blw_speed = Column(Float)
    brw_speed = Column(Float)
    fl_ride_height = Column(Float)
    fr_ride_height = Column(Float)
    bl_ride_height = Column(Float)
    br_ride_height = Column(Float)
    fl_strain_gauge_v = Column(Float)
    fr_strain_gauge_v = Column(Float)
    bl_strain_gauge_v = Column(Float)
    br_strain_gauge_v = Column(Float)
    fl_pushrod_stress = Column(Float)
    fr_pushrod_stress = Column(Float)
    bl_pushrod_stress = Column(Float)
    br_pushrod_stress = Column(Float)
    fl_spring_displace = Column(Float)
    fr_spring_displace = Column(Float)
    bl_spring_displace = Column(Float)
    br_spring_displace = Column(Float)
    dash_speed = Column(Float)
    f_gps = Column(ARRAY(Float))
    b_gps = Column(ARRAY(Float))
    f_gps_velocity = Column(Float)
    b_gps_velocity = Column(Float)
    f_gps_heading = Column(Float)
    b_gps_heading = Column(Float)
    inverter_v = Column(Float)
    inverter_c = Column(Float)
    inverter_rpm = Column(BigInteger)
    inverter_torque = Column(Float)
    packet = relationship("Packet", back_populates="dynamics")

class Controls(BaseTelemetry):
    __tablename__ = 'controls'
    packet_id = Column(BigInteger, ForeignKey('packet.packet_id'), primary_key=True)
    apps1_v = Column(Float)
    apps2_v = Column(Float)
    apps1_t = Column(Float)
    apps2_t = Column(Float)
    accel_pedal_t = Column(Float)
    bpps1_v = Column(Float)
    bpps2_v = Column(Float)
    bpps1_t = Column(Float)
    bpps2_t = Column(Float)
    brake_pedal_t = Column(Float)
    bse1_v = Column(Float)
    bse2_v = Column(Float)
    bse3_v = Column(Float)
    sus1_v = Column(Float)
    sus2_v = Column(Float)
    brake_pressure_f = Column(Float)
    brake_pressure_rbll = Column(Float)
    brake_pressure_rall = Column(Float)
    brake_bias = Column(Float)
    packet = relationship("Packet", back_populates="controls")

class Pack(BaseTelemetry):
    __tablename__ = 'pack'
    packet_id = Column(BigInteger, ForeignKey('packet.packet_id'), primary_key=True)
    hv_pack_v = Column(Float)
    hv_tractive_v = Column(Float)
    hv_c = Column(Float)
    lv_v = Column(Float)
    lv_c = Column(Float)
    contactor_state = Column(SmallInteger)
    avg_cell_v = Column(Float)
    avg_cell_temp = Column(Float)
    packet = relationship("Packet", back_populates="pack")

class DiagnosticsHigh(BaseTelemetry):
    __tablename__ = 'diagnostics_high'
    packet_id = Column(BigInteger, ForeignKey('packet.packet_id'), primary_key=True)
    apps1_disconnect = Column(Boolean)
    apps2_disconnect = Column(Boolean)
    apps1_out_range = Column(Boolean)
    apps2_out_range = Column(Boolean)
    apps_mismatch = Column(Boolean)
    apps_implause = Column(Boolean)
    bpps1_disconnect = Column(Boolean)
    bpps2_disconnect = Column(Boolean)
    bpps1_out_range = Column(Boolean)
    bpps2_out_range = Column(Boolean)
    bpps_mismatch = Column(Boolean)
    bse1_disconnect = Column(Boolean)
    bse2_disconnect = Column(Boolean)
    bse1_out_range = Column(Boolean)
    bse2_out_range = Column(Boolean)
    packet = relationship("Packet", back_populates="diagnostics_high")

class DiagnosticsLow(BaseTelemetry):
    __tablename__ = 'diagnostics_low'
    packet_id = Column(BigInteger, ForeignKey('packet.packet_id'), primary_key=True)
    batt_over_c = Column(Boolean)
    cell_over_v = Column(SmallInteger)
    cell_under_v = Column(SmallInteger)
    cell_open_wire = Column(SmallInteger)
    cell_damaged = Column(SmallInteger)
    thermistor_damaged = Column(SmallInteger)
    bmb_comm_error = Column(Boolean)
    imd_gnd_isolation_error = Column(Boolean)
    tractive_contactor_error = Column(Boolean)
    precharge_fail = Column(Boolean)
    cells_v_balanced = Column(Boolean)
    cell_min_v = Column(Float)
    cell_max_v = Column(Float)
    batt_v = Column(Float)
    batt_c = Column(Float)
    hv_soc = Column(Float)
    shutdown_leg1 = Column(Boolean)
    shutdown_leg2 = Column(Boolean)
    shutdown_leg3 = Column(Boolean)
    shutdown_leg4 = Column(Boolean)
    shutdown_leg5 = Column(Boolean)
    shutdown_leg6 = Column(Boolean)
    shutdown_leg7 = Column(Boolean)
    shutdown_leg8 = Column(Boolean)
    shutdown_leg9 = Column(Boolean)
    shutdown_leg10 = Column(Boolean)
    shutdown_leg11 = Column(Boolean)
    shutdown_leg12 = Column(Boolean)
    cells_temps = Column(ARRAY(Float))
    cells_v = Column(ARRAY(Float))
    packet = relationship("Packet", back_populates="diagnostics_low")

class Thermal(BaseTelemetry):
    __tablename__ = 'thermal'
    packet_id = Column(BigInteger, ForeignKey('packet.packet_id'), primary_key=True)
    motor_loop_flow_rate = Column(Float)
    motor_loop_motor_temp = Column(Float)
    motor_loop_inverter_temp = Column(Float)
    motor_loop_rad_temp = Column(Float)
    motor_loop_rad_fan_speed = Column(Float)
    ambient_temp = Column(Float)
    batt_loop_batt_temp = Column(Float)
    batt_loop_rad_temp = Column(Float)
    batt_loop_rad_fan_speed = Column(Float)
    motor_temp = Column(Float)
    inverter_temp = Column(Float)
    bus_bar_temp1 = Column(Float)
    bus_bar_temp2 = Column(Float)
    bus_bar_temp3 = Column(Float)
    precharge_r_temp = Column(Float)
    discharge_r_temp = Column(Float)
    batt_over_temp = Column(Boolean)
    packet = relationship("Packet", back_populates="thermal")

# -----------------------------
# Angelique Models (Generated)
# -----------------------------
# BEGIN GENERATED Angelique
class AngeliqueDynamics(BaseAngelique):
    __tablename__ = 'dynamics'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    torque_request = Column(Float)
    vcu_position = Column(ARRAY(Float))
    vcu_velocity = Column(ARRAY(Float))
    vcu_accel = Column(ARRAY(Float))
    gps = Column(ARRAY(Float))
    gps_velocity = Column(Float)
    gps_heading = Column(Float)
    body1_accel = Column(ARRAY(Float))
    body2_accel = Column(ARRAY(Float))
    body3_accel = Column(ARRAY(Float))
    flw_accel = Column(ARRAY(Float))
    frw_accel = Column(ARRAY(Float))
    blw_accel = Column(ARRAY(Float))
    brw_accel = Column(ARRAY(Float))
    body1_gyro = Column(ARRAY(Float))
    body2_gyro = Column(ARRAY(Float))
    body3_gyro = Column(ARRAY(Float))
    flw_speed = Column(Float)
    frw_speed = Column(Float)
    blw_speed = Column(Float)
    brw_speed = Column(Float)
    inverter_v = Column(Float)
    packet = relationship("AngeliquePacket", back_populates="dynamics")

class AngeliqueControls(BaseAngelique):
    __tablename__ = 'controls'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    vcu_flags = Column(BYTEA)
    vcu_flags_json = Column(JSONB)
    apps1_v = Column(Float)
    apps2_v = Column(Float)
    bse1_v = Column(Float)
    bse2_v = Column(Float)
    sus1_v = Column(Float)
    sus2_v = Column(Float)
    steer_v = Column(Float)
    packet = relationship("AngeliquePacket", back_populates="controls")

class AngeliqueDiagnostics(BaseAngelique):
    __tablename__ = 'diagnostics'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    current_errors = Column(BYTEA)
    current_errors_json = Column(JSONB)
    latching_faults = Column(BYTEA)
    latching_faults_json = Column(JSONB)
    cells_v = Column(ARRAY(Float))
    hv_charge_state = Column(Float)
    lv_charge_state = Column(Float)
    packet = relationship("AngeliquePacket", back_populates="diagnostics")

class AngeliqueThermal(BaseAngelique):
    __tablename__ = 'thermal'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    cells_temp = Column(ARRAY(SmallInteger))
    ambient_temp = Column(SmallInteger)
    inverter_temp = Column(SmallInteger)
    motor_temp = Column(SmallInteger)
    water_motor_temp = Column(SmallInteger)
    water_inverter_temp = Column(SmallInteger)
    water_rad_temp = Column(SmallInteger)
    rad_fan_set = Column(SmallInteger)
    rad_fan_rpm = Column(BigInteger)
    batt_fan_set = Column(SmallInteger)
    batt_fan_rpm = Column(SmallInteger)
    flow_rate = Column(SmallInteger)
    packet = relationship("AngeliquePacket", back_populates="thermal")

class AngeliquePack(BaseAngelique):
    __tablename__ = 'pack'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    hv_pack_v = Column(Float)
    hv_tractive_v = Column(Float)
    hv_c = Column(Float)
    lv_v = Column(Float)
    lv_c = Column(Float)
    contactor_state = Column(SmallInteger)
    avg_cell_v = Column(Float)
    avg_cell_temp = Column(Float)
    packet = relationship("AngeliquePacket", back_populates="pack")

class AngeliquePacket(BaseAngelique):
    __tablename__ = 'packet'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, primary_key=True)
    time = Column(BigInteger, nullable=False)
    dynamics = relationship("AngeliqueDynamics", uselist=False, back_populates="packet")
    controls = relationship("AngeliqueControls", uselist=False, back_populates="packet")
    pack = relationship("AngeliquePack", uselist=False, back_populates="packet")
    diagnostics = relationship("AngeliqueDiagnostics", uselist=False, back_populates="packet")
    thermal = relationship("AngeliqueThermal", uselist=False, back_populates="packet")
# END GENERATED Angelique

class Partitions(Base):
    __tablename__ = 'partitions'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    partition_name = Column(Text, primary_key=True)
    start_time = Column(BigInteger, primary_key=True)
    end_time = Column(BigInteger, nullable=False)

# BEGIN GENERATED Orion
class OrionPacket(BaseOrion):
    __tablename__ = 'packet'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, primary_key=True)
    time = Column(BigInteger, nullable=False)
    dynamics = relationship("OrionDynamics", uselist=False, back_populates="packet")
    controls = relationship("OrionControls", uselist=False, back_populates="packet")
    pack = relationship("OrionPack", uselist=False, back_populates="packet")
    diagnostics_high = relationship("OrionDiagnosticsHigh", uselist=False, back_populates="packet")
    diagnostics_low = relationship("OrionDiagnosticsLow", uselist=False, back_populates="packet")
    thermal = relationship("OrionThermal", uselist=False, back_populates="packet")
    board_status = relationship("OrionBoardStatus", uselist=False, back_populates="packet")

class OrionDynamics(BaseOrion):
    __tablename__ = 'dynamics'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    gps = Column(ARRAY(Float))
    gps_imu = Column(ARRAY(Float))
    gps_speed = Column(Float)
    accel_pedal_travel = Column(Float)
    steer_col_angle = Column(Float)
    bl_sprung_accel = Column(ARRAY(Float))
    bl_unsprung_accel = Column(ARRAY(Float))
    br_sprung_accel = Column(ARRAY(Float))
    br_unsprung_accel = Column(ARRAY(Float))
    fl_sprung_accel = Column(ARRAY(Float))
    fl_unsprung_accel = Column(ARRAY(Float))
    fr_sprung_accel = Column(ARRAY(Float))
    fr_unsprung_accel = Column(ARRAY(Float))
    bl_ride_height = Column(Float)
    bl_strain_gauge_v = Column(Float)
    bl_sus_pot_v = Column(Float)
    blw_speed = Column(Float)
    br_ride_height = Column(Float)
    br_strain_gauge_v = Column(Float)
    br_sus_pot_v = Column(Float)
    brw_speed = Column(Float)
    fl_ride_height = Column(Float)
    fl_strain_gauge_v = Column(Float)
    fl_sus_pot_v = Column(Float)
    flw_speed = Column(Float)
    fr_ride_height = Column(Float)
    fr_strain_gauge_v = Column(Float)
    fr_sus_pot_v = Column(Float)
    frw_speed = Column(Float)
    ride_height = Column(Float)
    wheel_speed = Column(Float)
    packet = relationship("OrionPacket", back_populates="dynamics")

class OrionControls(BaseOrion):
    __tablename__ = 'controls'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    motor_speed = Column(Float)
    torque_feedback = Column(Float)
    apps1_travel = Column(Float)
    apps1_v = Column(Float)
    apps2_travel = Column(Float)
    apps2_v = Column(Float)
    bpps1_travel = Column(Float)
    bpps1_v = Column(Float)
    bpps2_travel = Column(Float)
    bpps2_v = Column(Float)
    brake_bias = Column(Float)
    brake_light_pct = Column(Float)
    brake_pressure_f = Column(Float)
    brake_pressure_rall = Column(Float)
    brake_pressure_rbll = Column(Float)
    bse1_v = Column(Float)
    bse2_v = Column(Float)
    bse3_v = Column(Float)
    lights_current = Column(Float)
    rpm_request = Column(Float)
    torque_command = Column(Float)
    torque_limit = Column(Float)
    torque_request = Column(Float)
    commanded_torque = Column(Float)
    motor_angle = Column(Float)
    direction = Column(Boolean)
    enable = Column(Boolean)
    torque_shudder = Column(Float)
    packet = relationship("OrionPacket", back_populates="controls")

class OrionPack(BaseOrion):
    __tablename__ = 'pack'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    bus_voltage = Column(Float)
    lv_boards_current = Column(Float)
    cells_v = Column(ARRAY(Float))
    dc_bus_v = Column(Float)
    delta_resolver_angle = Column(Float)
    inverter_freq = Column(Float)
    neutral_output_v = Column(Float)
    time_since_on = Column(Float)
    vab_vq_v = Column(Float)
    vbc_vd_v = Column(Float)
    cells_temps = Column(ARRAY(Float))
    dc_bus_current = Column(Float)
    hv_c = Column(Float)
    hv_pack_v = Column(Float)
    hv_soc = Column(Float)
    lv_batt_c = Column(Float)
    lv_batt_t = Column(Float)
    lv_batt_v = Column(Float)
    phase_a_current = Column(Float)
    phase_b_current = Column(Float)
    phase_c_current = Column(Float)
    packet = relationship("OrionPacket", back_populates="pack")

class OrionDiagnosticsHigh(BaseOrion):
    __tablename__ = 'diagnostics_high'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    prndl_state = Column(Float)
    shutdown_current = Column(Float)
    hvc_state_machine = Column(Float)
    post_faults = Column(Float)
    run_faults = Column(Float)
    apps1_disconnect = Column(Boolean)
    apps1_out_range = Column(Boolean)
    apps2_disconnect = Column(Boolean)
    apps2_out_range = Column(Boolean)
    apps_implause = Column(Boolean)
    apps_mismatch = Column(Boolean)
    batt_fans_fuse = Column(Boolean)
    batt_pump_fuse = Column(Boolean)
    boards_fuse = Column(Boolean)
    bpps1_disconnect = Column(Boolean)
    bpps1_out_range = Column(Boolean)
    bpps2_disconnect = Column(Boolean)
    bpps2_out_range = Column(Boolean)
    bpps_mismatch = Column(Boolean)
    brake_light_fuse = Column(Boolean)
    bse1_disconnect = Column(Boolean)
    bse1_out_range = Column(Boolean)
    bse2_disconnect = Column(Boolean)
    bse2_out_range = Column(Boolean)
    ll_fuse = Column(Boolean)
    motor_pump_fuse = Column(Boolean)
    r2d_buzzer = Column(Boolean)
    rtd_fuse = Column(Boolean)
    shtdn_fuse = Column(Boolean)
    shutdown_bspd_status = Column(Boolean)
    shutdown_emeter_status = Column(Boolean)
    spare_fuse = Column(Boolean)
    stomp_fault = Column(Boolean)
    tssi_green_fuse = Column(Boolean)
    tssi_red_fuse = Column(Boolean)
    neg_hv_contactor = Column(Boolean)
    pos_hv_contactor = Column(Boolean)
    precharge_contactor = Column(Boolean)
    packet = relationship("OrionPacket", back_populates="diagnostics_high")

class OrionDiagnosticsLow(BaseOrion):
    __tablename__ = 'diagnostics_low'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    precharge_r_temp = Column(Float)
    bmb_comm_error = Column(Boolean)
    imd_gnd_isolation_error = Column(Boolean)
    r2d_authorized = Column(Boolean)
    r2d_status = Column(Boolean)
    shutdown_leg1 = Column(Boolean)
    shutdown_leg2 = Column(Boolean)
    shutdown_leg3 = Column(Boolean)
    shutdown_leg4 = Column(Boolean)
    temp_imd_1 = Column(Boolean)
    temp_imd_2 = Column(Boolean)
    temp_shutdown_1 = Column(Boolean)
    temp_shutdown_2 = Column(Boolean)
    packet = relationship("OrionPacket", back_populates="diagnostics_low")

class OrionThermal(BaseOrion):
    __tablename__ = 'thermal'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    batt_cooling_current = Column(Float)
    motor_cooling_current = Column(Float)
    ambient_temp = Column(Float)
    motor_temp = Column(Float)
    batt_loop_batt_temp = Column(Float)
    batt_loop_rad_fan_speed = Column(Float)
    batt_loop_rad_temp = Column(Float)
    battery_fan_rpm = Column(Float)
    bus_bar_temp1 = Column(Float)
    bus_bar_temp2 = Column(Float)
    bus_bar_temp3 = Column(Float)
    cell_bottom_temp = Column(Float)
    cell_top_temp = Column(Float)
    coolant_flow_lpm = Column(Float)
    coolant_temp = Column(Float)
    discharge_r_temp = Column(Float)
    fan_rpm = Column(Float)
    gate_driver_temp = Column(Float)
    inverter_hotspot_temp = Column(Float)
    inverter_temp = Column(Float)
    module_a_temp = Column(Float)
    module_b_temp = Column(Float)
    module_c_temp = Column(Float)
    motor_loop_inverter_temp = Column(Float)
    motor_loop_motor_temp = Column(Float)
    motor_loop_rad_temp = Column(Float)
    temp_ams_1 = Column(Boolean)
    temp_ams_2 = Column(Boolean)
    temp_command_1 = Column(Boolean)
    temp_command_2 = Column(Boolean)
    temp_output_1 = Column(Boolean)
    temp_output_2 = Column(Boolean)
    packet = relationship("OrionPacket", back_populates="thermal")

class OrionBoardStatus(BaseOrion):
    __tablename__ = 'board_status'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    csm_last_seen_s = Column(Float)
    dui_last_seen_s = Column(Float)
    hvc_last_seen_s = Column(Float)
    inverter_last_seen_s = Column(Float)
    pdu_last_seen_s = Column(Float)
    tsm_last_seen_s = Column(Float)
    usm_last_seen_s = Column(Float)
    vcu_last_seen_s = Column(Float)
    packet = relationship("OrionPacket", back_populates="board_status")

# END GENERATED Orion
