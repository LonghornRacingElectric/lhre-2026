CREATE ROLE grafana WITH
	LOGIN
	 PASSWORD 'frontend'
	CONNECTION LIMIT 10;
GRANT pg_read_all_data TO grafana;

CREATE ROLE analysis WITH
	LOGIN
	 PASSWORD 'north_dakota'
	CONNECTION LIMIT 10;
GRANT pg_read_all_data TO analysis;

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
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'0', E'Other', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'1', E'Rylan Hanks', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'2', E'Sohan Agnihotri', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'3', E'Dylan Hammerback', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'4', E'Andrew Cloran', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'5', E'Ali Jensen', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (E'6', E'David Easter', DEFAULT);

-- LUT for Location IDs
CREATE TABLE public.lut_location (
	location_id         smallint    NOT NULL,
	area                text        NOT NULL,
	track               text        NOT NULL,
	CONSTRAINT lut_location_pk PRIMARY KEY (location_id)
);
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'0', E'Other', E'Other');
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'1', E'Pickle', E'Innovation Blvd');
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'2', E'Pickle', E'North Lot');
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'3', E'Pickle', E'South Lot');
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'4', E'COTA', E'Lot J');
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'5', E'COTA', E'Lot H');
INSERT INTO public.lut_location (location_id, area, track) VALUES (E'6', E'COTA', E'Go Kart Track');


-- LUT for Car IDs
CREATE TABLE public.lut_car (
	car_id              smallint    NOT NULL,
	car_name            text        NOT NULL,
	CONSTRAINT lut_car_pk PRIMARY KEY (car_id)
);
INSERT INTO public.lut_car (car_id, car_name) VALUES (E'1', E'Easy Driver');
INSERT INTO public.lut_car (car_id, car_name) VALUES (E'2', E'Lady Luck');
INSERT INTO public.lut_car (car_id, car_name) VALUES (E'3', E'Angelique');
INSERT INTO public.lut_car (car_id, car_name) VALUES (E'4', E'Nightwatch');


-- LUT for Event Types
CREATE TABLE public.lut_event_type (
	type_id             smallint    NOT NULL,
	event_type          text        NOT NULL,
	CONSTRAINT lut_event_type_pk PRIMARY KEY (type_id)
);
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (E'0', E'Other');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (E'1', E'Endurance');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (E'2', E'Autocross');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (E'3', E'Skidpad');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (E'4', E'Straightline Acceleration');
INSERT INTO public.lut_event_type (type_id, event_type) VALUES (E'5', E'Straightline Breaking');

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
    -- Alignment
    camber_front             real,
    camber_rear              real,
    toe_front                real,
    toe_rear                 real,
    -- Ride height
    ride_height_front        real,
    ride_height_rear         real,
    -- Legacy single ride_height retained for back-compat
    ride_height              real,
    ackerman_adjustment      real,
    -- Legacy shock_dampening retained though unused in new UI
    shock_dampening          smallint,
    power_limit              integer,
    torque_limit             smallint,
    -- Tire cold pressures (existing columns)
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
    -- 2026 per-axle corner spring rates (available in Nightwatch)
    front_corner_spring_rate real,
    rear_corner_spring_rate  real,
    -- Note: Angelique-only specialty roll/heave springs are not present in Nightwatch schema
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

-- Packet Table
CREATE TABLE public.packet (
    packet_id           bigint   NOT NULL,
    "time"              bigint   NOT NULL,
    CONSTRAINT packet_pk PRIMARY KEY (packet_id)
);

-- Dynamics table
CREATE TABLE public.dynamics (
    packet_id           bigint   NOT NULL,
    steer_col_angle     float, 
    fl_steer_angle      float,
    fr_steer_angle      float,
    fl_sprung_accel     real[], 
    fr_sprung_accel     real[], 
    bl_sprung_accel     real[], 
    br_sprung_accel     real[],
    fl_unsprung_accel   real[], 
    fr_unsprung_accel   real[], 
    bl_unsprung_accel   real[], 
    br_unsprung_accel   real[], 
    fl_sprung_ang_rate  real[],
    fr_sprung_ang_rate  real[], 
    bl_sprung_ang_rate  real[], 
    br_sprung_ang_rate  real[], 
    cent_mass_accel     real[], 
    cent_mass_ang_rate  real[], 
    flw_speed           real,
    frw_speed           real,
    blw_speed           real,
    brw_speed           real,
    fl_ride_height      real, 
    fr_ride_height      real,
    bl_ride_height      real,
    br_ride_height      real,
    fl_strain_gauge_v   real, 
    fr_strain_gauge_v   real,
    bl_strain_gauge_v   real,
    br_strain_gauge_v   real,
    fl_pushrod_stress   real, 
    fr_pushrod_stress   real,
    bl_pushrod_stress   real,
    br_pushrod_stress   real,
    fl_spring_displace  real, 
    fr_spring_displace  real, 
    bl_spring_displace  real, 
    br_spring_displace  real,
    dash_speed          real, 
    f_gps               point, 
    b_gps               point,
    f_gps_velocity      real,
    b_gps_velocity      real,
    f_gps_heading       real,
    b_gps_heading       real,
    inverter_v          real,
    inverter_c          real,
    inverter_rpm        bigint,
    inverter_torque     real,
    CONSTRAINT fk_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Controls table
CREATE TABLE public.controls (
    packet_id           bigint   NOT NULL,
    apps1_v             real,
    apps2_v             real,
    apps1_t             real,
    apps2_t             real,
    accel_pedal_t       real,
    bpps1_v             real,
    bpps2_v             real,
    bpps1_t             real,
    bpps2_t             real,
    brake_pedal_t       real,
    bse1_v              real,
    bse2_v              real,
    bse3_v              real, 
    sus1_v              real,
    sus2_v              real,
    brake_pressure_f    real,
    brake_pressure_rbll real, 
    brake_pressure_rall real,
    brake_bias          real,
    CONSTRAINT fk_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Pack table
CREATE TABLE public.pack (
    -- Not updated from Sensor Table
    packet_id           bigint   NOT NULL,
    hv_pack_v           real,
    hv_tractive_v       real,
    hv_c                real,
    lv_v                real,
    lv_c                real,
    contactor_state     smallint,
    avg_cell_v          real,
    avg_cell_temp       real,
    CONSTRAINT fk_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);


-- High Frequency Diangostics Table
CREATE TABLE public.diagnostics_high (
    packet_id               bigint   NOT NULL,
    apps1_disconnect        boolean,
    apps2_disconnect        boolean,
    apps1_out_range         boolean,
    apps2_out_range         boolean,
    apps_mismatch           boolean, 
    apps_implause           boolean,
    bpps1_disconnect        boolean,
    bpps2_disconnect        boolean,
    bpps1_out_range         boolean, 
    bpps2_out_range         boolean,
    bpps_mismatch           boolean,
    bse1_disconnect         boolean,
    bse2_disconnect         boolean,
    bse1_out_range          boolean,
    bse2_out_range          boolean,
    CONSTRAINT fk_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);


-- Low Frequency Diagnostics table
CREATE TABLE public.diagnostics_low (
    packet_id               bigint   NOT NULL,
    batt_over_c             boolean, 
    cell_over_v             smallint,
    cell_under_v            smallint,
    cell_open_wire          smallint,
    cell_damaged            smallint,
    thermistor_damaged      smallint,
    bmb_comm_error          boolean, 
    imd_gnd_isolation_error boolean,
    tractive_contactor_error boolean,
    precharge_fail          boolean,
    cells_v_balanced        boolean, 
    cell_min_v              real, 
    cell_max_v              real,
    batt_v                  real, 
    batt_c                  real,
    hv_soc                  real, 
    shutdown_leg1           boolean, 
    shutdown_leg2           boolean, 
    shutdown_leg3           boolean, 
    shutdown_leg4           boolean,
    shutdown_leg5           boolean, 
    shutdown_leg6           boolean, 
    shutdown_leg7           boolean, 
    shutdown_leg8           boolean,
    shutdown_leg9           boolean, 
    shutdown_leg10          boolean, 
    shutdown_leg11          boolean, 
    shutdown_leg12          boolean,
    cells_temps             real[],
    cells_v                 real[],
    CONSTRAINT fk_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Thermal table
CREATE TABLE public.thermal
(
    packet_id           bigint   NOT NULL,
    motor_loop_flow_rate    real,
    motor_loop_motor_temp   real,
    motor_loop_inverter_temp real,
    motor_loop_rad_temp     real,
    motor_loop_rad_fan_speed real,
    ambient_temp            real,
    batt_loop_batt_temp     real,
    batt_loop_rad_temp      real,
    batt_loop_rad_fan_speed real, 
    motor_temp              real, 
    inverter_temp           real,
    bus_bar_temp1           real, 
    bus_bar_temp2           real, 
    bus_bar_temp3           real, 
    precharge_r_temp        real, 
    discharge_r_temp        real,
    batt_over_temp          boolean, 
    CONSTRAINT fk_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
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