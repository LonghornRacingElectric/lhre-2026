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
    torque_request      real,
    vcu_position        real[],
    vcu_velocity        real[],
    vcu_accel           real[],
    gps                 real[],
    gps_velocity        real,
    gps_heading         real,
    body1_accel         real[],
    body2_accel         real[],
    body3_accel         real[],
    flw_accel           real[],
    frw_accel           real[],
    blw_accel           real[],
    brw_accel           real[],
    body1_gyro          real[],
    body2_gyro          real[],
    body3_gyro          real[],
    flw_speed           real,
    frw_speed           real,
    blw_speed           real,
    brw_speed           real,
    inverter_v          real,
    inverter_c          real,
    inverter_rpm        smallint,
    inverter_torque     real,
    CONSTRAINT fk_dynamics_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Controls table
CREATE TABLE public.controls (
    packet_id           bigint   NOT NULL,
    vcu_flags           bytea,
    vcu_flags_json      jsonb,
    apps1_v             real,
    apps2_v             real,
    bse1_v              real,
    bse2_v              real,
    sus1_v              real,
    sus2_v              real,
    steer_v             real,
    CONSTRAINT fk_controls_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Pack table
CREATE TABLE public.pack (
    packet_id           bigint   NOT NULL,
    hv_pack_v           real,
    hv_tractive_v       real,
    hv_c                real,
    lv_v                real,
    lv_c                real,
    contactor_state     smallint,
    avg_cell_v          real,
    avg_cell_temp       real,
    CONSTRAINT fk_pack_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);


-- Diagnostics table
CREATE TABLE public.diagnostics (
    packet_id               bigint   NOT NULL,
    current_errors          bytea,
    current_errors_json     jsonb,
    latching_faults         bytea,
    latching_faults_json    jsonb,
    cells_v                 real[],
    hv_charge_state         real,
    lv_charge_state         real,
    odometer                real,
    CONSTRAINT fk_diagnostics_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Thermal table
CREATE TABLE public.thermal
(
    packet_id           bigint   NOT NULL,
    cells_temp          smallint[],
    ambient_temp        smallint,
    inverter_temp       smallint,
    motor_temp          smallint,
    water_motor_temp    smallint,
    water_inverter_temp smallint,
    water_rad_temp      smallint,
    rad_fan_set         smallint,
    rad_fan_rpm         bigint,
    batt_fan_set        smallint,
    batt_fan_rpm        smallint,
    flow_rate           smallint,
    CONSTRAINT fk_thermal_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
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


CREATE OR REPLACE FUNCTION public.get_partition_bounds(
    p_partition_name text,
    p_time_from timestamptz DEFAULT NULL,
    p_time_to timestamptz DEFAULT NULL,
    p_bucket_divisor double precision DEFAULT 5000.0
)
RETURNS TABLE(
    bucket_len double precision,
    effective_start bigint,
    effective_end bigint
)
LANGUAGE sql
STABLE
AS
$$
    SELECT
        GREATEST(
            (
                LEAST(p.end_time, COALESCE((EXTRACT(EPOCH FROM p_time_to) * 1000)::bigint, p.end_time))
                - GREATEST(p.start_time, COALESCE((EXTRACT(EPOCH FROM p_time_from) * 1000)::bigint, p.start_time))
            ) / p_bucket_divisor,
            1
        ) AS bucket_len,
        GREATEST(p.start_time, COALESCE((EXTRACT(EPOCH FROM p_time_from) * 1000)::bigint, p.start_time)) AS effective_start,
        LEAST(p.end_time, COALESCE((EXTRACT(EPOCH FROM p_time_to) * 1000)::bigint, p.end_time)) AS effective_end
    FROM partitions p
    WHERE p.partition_name = p_partition_name;
$$;

ALTER FUNCTION public.get_partition_bounds(text,timestamptz,timestamptz,double precision) OWNER TO electric;