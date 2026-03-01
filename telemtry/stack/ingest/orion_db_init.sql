-- Functions
CREATE OR REPLACE FUNCTION public.get_event_index (car smallint, day smallint)
	RETURNS smallint
	LANGUAGE plpgsql
	IMMUTABLE
	STRICT
	AS
$$
DECLARE ind smallint;
BEGIN
	SELECT COUNT(event_id)
	INTO ind
	FROM event
	WHERE car_id = car AND day_id = day;
	RETURN ind + 1;
END;
$$;

ALTER FUNCTION public.get_event_index(smallint,smallint) OWNER TO electric;

-- Drive Day Table
CREATE TABLE public.drive_day (
	day_id              smallserial NOT NULL,
	date                date        NOT NULL,
	power_limit         integer,
	air_temperature         real,
    relative_humidity       real,
    track_temperature       real,
	CONSTRAINT drive_day_pk PRIMARY KEY (day_id)
);

-- LUT for Driver IDs
CREATE TABLE public.lut_driver (
	driver_id           smallint    NOT NULL,
	driver_name         text        NOT NULL,
	driver_weight       smallint,
	CONSTRAINT lut_driver_pk PRIMARY KEY (driver_id)
);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (0, 'Other', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (1, 'Rylan Hanks', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (2, 'Sohan Agnihotri', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (3, 'Dylan Hammerback', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (4, 'Andrew Cloran', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (5, 'Ali Jensen', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (6, 'David Easter', DEFAULT);

-- LUT for Location IDs
CREATE TABLE public.lut_location (
	location_id         smallint    NOT NULL,
	area                text        NOT NULL,
	track               text        NOT NULL,
	CONSTRAINT lut_location_pk PRIMARY KEY (location_id)
);
INSERT INTO public.lut_location (location_id, area, track) VALUES (0, 'Other', 'Other');
INSERT INTO public.lut_location (location_id, area, track) VALUES (1, 'Pickle', 'Innovation Blvd');
INSERT INTO public.lut_location (location_id, area, track) VALUES (2, 'Pickle', 'North Lot');
INSERT INTO public.lut_location (location_id, area, track) VALUES (3, 'Pickle', 'South Lot');
INSERT INTO public.lut_location (location_id, area, track) VALUES (4, 'COTA', 'Lot J');
INSERT INTO public.lut_location (location_id, area, track) VALUES (5, 'COTA', 'Lot H');
INSERT INTO public.lut_location (location_id, area, track) VALUES (6, 'COTA', 'Go Kart Track');


-- LUT for Car IDs
CREATE TABLE public.lut_car (
	car_id              smallint    NOT NULL,
	car_name            text        NOT NULL,
	CONSTRAINT lut_car_pk PRIMARY KEY (car_id)
);
INSERT INTO public.lut_car (car_id, car_name) VALUES (1, 'Easy Driver');
INSERT INTO public.lut_car (car_id, car_name) VALUES (2, 'Lady Luck');
INSERT INTO public.lut_car (car_id, car_name) VALUES (3, 'Angelique');
INSERT INTO public.lut_car (car_id, car_name) VALUES (4, 'Nightwatch');


-- LUT for Event Types
CREATE TABLE public.lut_event_type (
	type_id             smallint    NOT NULL,
	event_type          text        NOT NULL,
	CONSTRAINT lut_event_type_pk PRIMARY KEY (type_id)
);
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (0, 'Other');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (1, 'Endurance');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (2, 'Autocross');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (3, 'Skidpad');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (4, 'Straightline Acceleration');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (5, 'Straightline Breaking');

-- Event Table
CREATE TABLE public.event (
    event_id                 smallserial NOT NULL,
    day_id                   smallint    NOT NULL,
    status                   smallint,
    creation_time            bigint      NOT NULL,
    start_time               bigint,
    end_time                 bigint,
    packet_start             bigint,
    packet_end               bigint,
    car_id                   smallint    NOT NULL,
    driver_id                smallint    NOT NULL,
    location_id              smallint    NOT NULL,
    event_type               smallint    NOT NULL,
    event_index              smallint    GENERATED ALWAYS AS (public.get_event_index(car_id, day_id)) STORED,
    car_weight               smallint,
    tow_angle                real,
    -- Alignment per axle
    camber_front             real,
    camber_rear              real,
    toe_front                real,
    toe_rear                 real,
    -- Ride height per axle (+ legacy)
    ride_height_front        real,
    ride_height_rear         real,
    ride_height              real,
    ackerman_adjustment      real,
    shock_dampening          smallint,
    power_limit              integer,
    torque_limit             smallint,
    -- Tire cold pressures
    frw_pressure             real,
    flw_pressure             real,
    brw_pressure             real,
    blw_pressure             real,
    -- Tire wear depth and durometer
    fr_wear_depth            real,
    fl_wear_depth            real,
    rr_wear_depth            real,
    rl_wear_depth            real,
    fr_durometer             real,
    fl_durometer             real,
    rr_durometer             real,
    rl_durometer             real,
    -- Shock damping per corner (LSC/LSR/HSC/HSR)
    fr_lsc                   smallint,
    fr_lsr                   smallint,
    fr_hsc                   smallint,
    fr_hsr                   smallint,
    fl_lsc                   smallint,
    fl_lsr                   smallint,
    fl_hsc                   smallint,
    fl_hsr                   smallint,
    rr_lsc                   smallint,
    rr_lsr                   smallint,
    rr_hsc                   smallint,
    rr_hsr                   smallint,
    rl_lsc                   smallint,
    rl_lsr                   smallint,
    rl_hsc                   smallint,
    rl_hsr                   smallint,
    -- Aero
    front_wing_on            boolean,
    rear_wing_on             boolean,
    front_wing_pitch         real,
    rear_wing_pitch          real,
    regen_on                 boolean,
    undertray_on             boolean,
    -- Angelique-only specialty springs
    front_roll_spring_rate   real,
    front_heave_spring_rate  real,
    rear_roll_spring_rate    real,
    rear_heave_spring_rate   real,
    -- 2026 car-only fields (per axle)
    front_corner_spring_rate real,
    rear_corner_spring_rate  real,
    -- ARB settings (free text: low|medium|stiff)
    front_arb_setting        text,
    rear_arb_setting         text,
    CONSTRAINT event_pk PRIMARY KEY (event_id),
    CONSTRAINT fk_event_id FOREIGN KEY(day_id) REFERENCES drive_day(day_id),
    CONSTRAINT fk_car_id FOREIGN KEY(car_id) REFERENCES lut_car(car_id),
    CONSTRAINT fk_driver_id FOREIGN KEY(driver_id) REFERENCES lut_driver(driver_id),
    CONSTRAINT fk_location_id FOREIGN KEY(location_id) REFERENCES lut_location(location_id),
    CONSTRAINT fk_event_type FOREIGN KEY(event_type) REFERENCES lut_event_type(type_id)
);

-- Classifier table
CREATE TABLE public.classifier (
    event_id            bigint      NOT NULL,
    type                text        NOT NULL,
    start_time          bigint      NOT NULL,
    end_time            bigint,
    notes               text,
    CONSTRAINT fk_event_id FOREIGN KEY(event_id) REFERENCES event(event_id)
);

-- Partitions table
CREATE TABLE public.partitions(
    partition_name    text         NOT NULL,
    start_time        bigint       NOT NULL,
    end_time          bigint       NOT NULL
);
-- Generated Packet Table
CREATE TABLE public.packet (
    packet_id           bigint   NOT NULL,
    "time"              bigint   NOT NULL,
    CONSTRAINT packet_pk PRIMARY KEY (packet_id)
);

-- Generated Dynamics Table
CREATE TABLE public.dynamics (
    packet_id           bigint   NOT NULL,
    accel_pedal_travel   real,
    steer_col_angle      real,
    bl_sprung_accel      real[],
    bl_unsprung_accel    real[],
    br_sprung_accel      real[],
    br_unsprung_accel    real[],
    fl_sprung_accel      real[],
    fl_unsprung_accel    real[],
    fr_sprung_accel      real[],
    fr_unsprung_accel    real[],
    bl_ride_height       real,
    bl_strain_gauge_v    real,
    bl_sus_pot_v         real,
    blw_speed            real,
    br_ride_height       real,
    br_strain_gauge_v    real,
    br_sus_pot_v         real,
    brw_speed            real,
    fl_ride_height       real,
    fl_strain_gauge_v    real,
    fl_sus_pot_v         real,
    flw_speed            real,
    fr_ride_height       real,
    fr_strain_gauge_v    real,
    fr_sus_pot_v         real,
    frw_speed            real,
    ride_height          real,
    wheel_speed          real,
    CONSTRAINT fk_dynamics_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Controls Table
CREATE TABLE public.controls (
    packet_id           bigint   NOT NULL,
    motor_speed          real,
    torque_feedback      real,
    apps1_travel         real,
    apps1_v              real,
    apps2_travel         real,
    apps2_v              real,
    bpps1_travel         real,
    bpps1_v              real,
    bpps2_travel         real,
    bpps2_v              real,
    brake_bias           real,
    brake_light_pct      real,
    brake_pressure_f     real,
    brake_pressure_rall  real,
    brake_pressure_rbll  real,
    bse1_v               real,
    bse2_v               real,
    bse3_v               real,
    lights_current       real,
    rpm_request          real,
    torque_command       real,
    torque_limit         real,
    torque_request       real,
    commanded_torque     real,
    motor_angle          real,
    direction            boolean,
    enable               boolean,
    torque_shudder       real,
    CONSTRAINT fk_controls_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Pack Table
CREATE TABLE public.pack (
    packet_id           bigint   NOT NULL,
    bus_voltage          real,
    lv_boards_current    real,
    cells_v              real[],
    dc_bus_v             real,
    delta_resolver_angle real,
    inverter_freq        real,
    neutral_output_v     real,
    time_since_on        real,
    vab_vq_v             real,
    vbc_vd_v             real,
    cells_temps          real[],
    dc_bus_current       real,
    hv_c                 real,
    hv_pack_v            real,
    hv_soc               real,
    lv_batt_c            real,
    lv_batt_t            real,
    lv_batt_v            real,
    phase_a_current      real,
    phase_b_current      real,
    phase_c_current      real,
    CONSTRAINT fk_pack_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Diagnostics_high Table
CREATE TABLE public.diagnostics_high (
    packet_id           bigint   NOT NULL,
    shutdown_current     real,
    hvc_state_machine    real,
    post_faults          real,
    run_faults           real,
    neg_hv_contactor     boolean,
    pos_hv_contactor     boolean,
    precharge_contactor  boolean,
    CONSTRAINT fk_diagnostics_high_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Diagnostics_low Table
CREATE TABLE public.diagnostics_low (
    packet_id           bigint   NOT NULL,
    precharge_r_temp     real,
    bmb_comm_error       boolean,
    imd_gnd_isolation_error boolean,
    r2d_authorized       boolean,
    r2d_status           boolean,
    shutdown_leg1        boolean,
    shutdown_leg2        boolean,
    shutdown_leg3        boolean,
    shutdown_leg4        boolean,
    CONSTRAINT fk_diagnostics_low_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Thermal Table
CREATE TABLE public.thermal (
    packet_id           bigint   NOT NULL,
    batt_cooling_current real,
    motor_cooling_current real,
    motor_temp           real,
    ambient_temp         real,
    batt_loop_batt_temp  real,
    batt_loop_rad_fan_speed real,
    batt_loop_rad_temp   real,
    bus_bar_temp1        real,
    bus_bar_temp2        real,
    bus_bar_temp3        real,
    cell_bottom_temp     real,
    cell_top_temp        real,
    discharge_r_temp     real,
    gate_driver_temp     real,
    inverter_temp        real,
    module_a_temp        real,
    module_b_temp        real,
    module_c_temp        real,
    motor_loop_inverter_temp real,
    motor_loop_motor_temp real,
    motor_loop_rad_fan_speed real,
    motor_loop_rad_temp  real,
    rtd4_temp            real,
    rtd5_temp            real,
    CONSTRAINT fk_thermal_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);
