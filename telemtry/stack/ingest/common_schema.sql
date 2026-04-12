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
	track_name              text,
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
    event_id     integer   NOT NULL,
    latitude     real      NOT NULL,
    longitude    real      NOT NULL,
    timestamp_ms bigint    NOT NULL,
    CONSTRAINT track_point_pk PRIMARY KEY (point_id),
    CONSTRAINT fk_event_id FOREIGN KEY (event_id) REFERENCES event(event_id)
);
CREATE INDEX track_point_event_idx ON public.track_point (event_id);
