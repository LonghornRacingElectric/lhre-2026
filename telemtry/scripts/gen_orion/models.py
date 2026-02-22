class OrionPacket(BaseOrion):
    __tablename__ = 'packet'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, primary_key=True)
    time = Column(BigInteger, nullable=False)
    dynamics = relationship("OrionDynamics", uselist=False, back_populates="packet")
    controls = relationship("OrionControls", uselist=False, back_populates="packet")
    pack = relationship("OrionPack", uselist=False, back_populates="packet")
    diagnostics_low = relationship("OrionDiagnosticsLow", uselist=False, back_populates="packet")
    thermal = relationship("OrionThermal", uselist=False, back_populates="packet")

class OrionDynamics(BaseOrion):
    __tablename__ = 'dynamics'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
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
    packet = relationship("OrionPacket", back_populates="dynamics")

class OrionControls(BaseOrion):
    __tablename__ = 'controls'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    apps1_travel = Column(Float)
    apps1_v = Column(Float)
    apps2_travel = Column(Float)
    apps2_v = Column(Float)
    bpps1_travel = Column(Float)
    bpps1_v = Column(Float)
    bpps2_travel = Column(Float)
    bpps2_v = Column(Float)
    brake_bias = Column(Float)
    brake_pressure_f = Column(Float)
    brake_pressure_rall = Column(Float)
    brake_pressure_rbll = Column(Float)
    bse1_v = Column(Float)
    bse2_v = Column(Float)
    bse3_v = Column(Float)
    rpm_request = Column(Float)
    torque_request = Column(Float)
    packet = relationship("OrionPacket", back_populates="controls")

class OrionPack(BaseOrion):
    __tablename__ = 'pack'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    dc_bus_v = Column(Float)
    hv_c = Column(Float)
    hv_pack_v = Column(Float)
    hv_soc = Column(Float)
    lv_batt_c = Column(Float)
    lv_batt_t = Column(Float)
    lv_batt_v = Column(Float)
    torque_command = Column(Float)
    packet = relationship("OrionPacket", back_populates="pack")

class OrionDiagnosticsLow(BaseOrion):
    __tablename__ = 'diagnostics_low'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    precharge_r_temp = Column(Float)
    bmb_comm_error = Column(Boolean)
    imd_gnd_isolation_error = Column(Boolean)
    shutdown_leg1 = Column(Boolean)
    shutdown_leg2 = Column(Boolean)
    shutdown_leg3 = Column(Boolean)
    shutdown_leg4 = Column(Boolean)
    packet = relationship("OrionPacket", back_populates="diagnostics_low")

class OrionThermal(BaseOrion):
    __tablename__ = 'thermal'
    __table_args__ = {'schema': 'public', 'extend_existing': True}
    packet_id = Column(BigInteger, ForeignKey('public.packet.packet_id'), primary_key=True)
    cells_v = Column(ARRAY(Float))
    cells_temps = Column(ARRAY(Float))
    ambient_temp = Column(Float)
    batt_loop_batt_temp = Column(Float)
    batt_loop_rad_fan_speed = Column(Float)
    batt_loop_rad_temp = Column(Float)
    bus_bar_temp1 = Column(Float)
    bus_bar_temp2 = Column(Float)
    bus_bar_temp3 = Column(Float)
    discharge_r_temp = Column(Float)
    inverter_temp = Column(Float)
    motor_loop_inverter_temp = Column(Float)
    motor_loop_motor_temp = Column(Float)
    motor_loop_rad_fan_speed = Column(Float)
    motor_loop_rad_temp = Column(Float)
    motor_temp = Column(Float)
    packet = relationship("OrionPacket", back_populates="thermal")
