-- Drive Day Table
CREATE TABLE public.drive_day (
    day_id                   smallserial NOT NULL,
    date                     date        NOT NULL,
    track_name               text,
    weather                  text,
    wind_speed               real,
    power_limit              integer,
    air_temperature          real,
    relative_humidity        real,
    track_temperature        real,
    -- Status and timing
    status                   smallint,
    creation_time            bigint,
    start_time               bigint,
    end_time                 bigint,
    packet_start             bigint,
    packet_end               bigint,
    -- Driver, car, location, event type
    car_id                   smallint,
    driver_id                smallint,
    location_id              smallint,
    event_type               smallint,
    -- Car setup
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
    torque_limit             smallint,
    -- Tire cold pressures
    frw_pressure             real,
    flw_pressure             real,
    brw_pressure             real,
    blw_pressure             real,
    -- Tire hot pressures
    frw_hot_pressure         real,
    flw_hot_pressure         real,
    brw_hot_pressure         real,
    blw_hot_pressure         real,
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
    CONSTRAINT drive_day_pk  PRIMARY KEY (day_id)
);

-- LUT for Driver IDs
CREATE TABLE public.lut_driver (
	driver_id           smallint    NOT NULL,
	driver_name         text        NOT NULL,
	driver_weight       smallint,
	CONSTRAINT lut_driver_pk PRIMARY KEY (driver_id)
);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (0, 'Other', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (4, 'Andrew Cloran', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (5, 'Ali Jensen', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (7, 'Viraj Bhalla', DEFAULT);
INSERT INTO public.lut_driver (driver_id, driver_name, driver_weight) VALUES (8, 'Luke Ballengee', DEFAULT);

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
INSERT INTO public.lut_car (car_id, car_name) VALUES (5, 'Orion');


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

ALTER TABLE public.drive_day
    ADD CONSTRAINT fk_car_id FOREIGN KEY (car_id) REFERENCES public.lut_car(car_id),
    ADD CONSTRAINT fk_driver_id FOREIGN KEY (driver_id) REFERENCES public.lut_driver(driver_id),
    ADD CONSTRAINT fk_location_id FOREIGN KEY (location_id) REFERENCES public.lut_location(location_id),
    ADD CONSTRAINT fk_event_type FOREIGN KEY (event_type) REFERENCES public.lut_event_type(type_id);


-- Track Mapping table
CREATE TABLE public.track_mapping (
    track_mapping_id    serial          NOT NULL,
    name                text            NOT NULL,
    created_at          bigint          NOT NULL,
    start_gate_lat1     double precision NOT NULL,
    start_gate_lon1     double precision NOT NULL,
    start_gate_lat2     double precision NOT NULL,
    start_gate_lon2     double precision NOT NULL,
    points              jsonb,
    sectors             jsonb,
    CONSTRAINT track_mapping_pk PRIMARY KEY (track_mapping_id)
);

-- Classifier table
CREATE TABLE public.classifier (
    day_id              bigint      NOT NULL,
    type                text        NOT NULL,
    start_time          bigint      NOT NULL,
    end_time            bigint,
    notes               text,
    CONSTRAINT fk_day_id FOREIGN KEY(day_id) REFERENCES drive_day(day_id)
);

-- Partitions table
CREATE TABLE public.partitions(
    partition_name    text         NOT NULL,
    start_time        bigint       NOT NULL,
    end_time          bigint       NOT NULL
);

-- Car Status Segment table
-- Written by the car_status processor: one row per closed state segment
-- (OFF / ON_IDLE / READY / MOVING). Standalone — keyed by car + time range, no
-- drive_day dependency. active_faults are advisory metadata, not the state.
CREATE TABLE public.car_status_segment (
    segment_id        bigserial    NOT NULL,
    car               text         NOT NULL,
    state             text         NOT NULL,
    start_time        bigint       NOT NULL,
    end_time          bigint,
    start_packet      bigint,
    end_packet        bigint,
    hv_soc_avg        real,
    lv_v_avg          real,
    active_faults     text,
    CONSTRAINT car_status_segment_pk PRIMARY KEY (segment_id)
);
CREATE INDEX idx_car_status_segment_car_time ON public.car_status_segment (car, start_time DESC);
CREATE INDEX idx_car_status_segment_state ON public.car_status_segment (state);

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

-- Track mapping tables
CREATE TABLE public.track (
    track_id    serial      NOT NULL,
    name        text        NOT NULL,
    location_id integer,
    created_at  timestamptz NOT NULL DEFAULT now(),
    CONSTRAINT track_pk PRIMARY KEY (track_id),
    CONSTRAINT track_name_unique UNIQUE (name),
    CONSTRAINT fk_location_id FOREIGN KEY (location_id) REFERENCES lut_location(location_id)
);
CREATE INDEX track_location_idx ON public.track (location_id);

CREATE TABLE public.sector_gate (
    gate_id    serial  NOT NULL,
    track_id   integer NOT NULL,
    gate_index integer NOT NULL,
    lat1       real    NOT NULL,
    lon1       real    NOT NULL,
    lat2       real    NOT NULL,
    lon2       real    NOT NULL,
    CONSTRAINT sector_gate_pk PRIMARY KEY (gate_id),
    CONSTRAINT sector_gate_unique UNIQUE (track_id, gate_index),
    CONSTRAINT fk_track_id FOREIGN KEY (track_id) REFERENCES track(track_id)
);
CREATE INDEX sector_gate_track_idx ON public.sector_gate (track_id);

CREATE TABLE public.track_point (
    point_id     bigserial NOT NULL,
    day_id       integer   NOT NULL,
    latitude     real      NOT NULL,
    longitude    real      NOT NULL,
    timestamp_ms bigint    NOT NULL,
    CONSTRAINT track_point_pk PRIMARY KEY (point_id),
    CONSTRAINT fk_day_id FOREIGN KEY (day_id) REFERENCES drive_day(day_id)
);
CREATE INDEX track_point_day_idx ON public.track_point (day_id);
-- Generated Packet Table
CREATE TABLE public.packet (
    packet_id           bigint   NOT NULL,
    "time"              bigint   NOT NULL,
    CONSTRAINT packet_pk PRIMARY KEY (packet_id)
);

-- Generated Dynamics Table
CREATE TABLE public.dynamics (
    packet_id           bigint   NOT NULL,
    torque_request       real,
    vcu_position         real[],
    vcu_velocity         real[],
    vcu_accel            real[],
    gps                  real[],
    gps_velocity         real,
    gps_heading          real,
    body1_accel          real[],
    body2_accel          real[],
    body3_accel          real[],
    flw_accel            real[],
    frw_accel            real[],
    blw_accel            real[],
    brw_accel            real[],
    body1_gyro           real[],
    body2_gyro           real[],
    body3_gyro           real[],
    flw_speed            real,
    frw_speed            real,
    blw_speed            real,
    brw_speed            real,
    inverter_v           real,
    inverter_c           real,
    inverter_rpm         integer,
    inverter_torque      real,
    CONSTRAINT fk_dynamics_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Controls Table
CREATE TABLE public.controls (
    packet_id           bigint   NOT NULL,
    vcu_flags            bytea,
    vcu_flags_json       jsonb,
    apps1_v              real,
    apps2_v              real,
    bse1_v               real,
    bse2_v               real,
    sus1_v               real,
    sus2_v               real,
    steer_v              real,
    CONSTRAINT fk_controls_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Pack Table
CREATE TABLE public.pack (
    packet_id           bigint   NOT NULL,
    hv_pack_v            real,
    hv_tractive_v        real,
    hv_c                 real,
    lv_v                 real,
    lv_c                 real,
    contactor_state      integer,
    avg_cell_v           real,
    avg_cell_temp        real,
    CONSTRAINT fk_pack_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Diagnostics Table
CREATE TABLE public.diagnostics (
    packet_id           bigint   NOT NULL,
    current_errors       bytea,
    current_errors_json  jsonb,
    latching_faults      bytea,
    latching_faults_json jsonb,
    cells_v              real[],
    hv_charge_state      real,
    lv_charge_state      real,
    CONSTRAINT fk_diagnostics_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Thermal Table
CREATE TABLE public.thermal (
    packet_id           bigint   NOT NULL,
    cells_temp           integer[],
    ambient_temp         integer,
    inverter_temp        integer,
    motor_temp           integer,
    water_motor_temp     integer,
    water_inverter_temp  integer,
    water_rad_temp       integer,
    rad_fan_set          integer,
    rad_fan_rpm          bigint,
    batt_fan_set         integer,
    batt_fan_rpm         integer,
    flow_rate            integer,
    CONSTRAINT fk_thermal_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Indexes
CREATE INDEX IF NOT EXISTS idx_packet_time ON public.packet ("time" DESC);
CREATE INDEX IF NOT EXISTS idx_dynamics_packet_id ON public.dynamics (packet_id);
CREATE INDEX IF NOT EXISTS idx_controls_packet_id ON public.controls (packet_id);
CREATE INDEX IF NOT EXISTS idx_pack_packet_id ON public.pack (packet_id);
CREATE INDEX IF NOT EXISTS idx_diagnostics_packet_id ON public.diagnostics (packet_id);
CREATE INDEX IF NOT EXISTS idx_thermal_packet_id ON public.thermal (packet_id);
