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
    gps                  real[],
    gps_imu              real[],
    gps_speed            real,
    bl_unsprung_accel    real[],
    br_unsprung_accel    real[],
    fl_unsprung_accel    real[],
    fr_unsprung_accel    real[],
    accel_pedal_travel   real,
    bl_wheel_speed       real,
    br_wheel_speed       real,
    fl_wheel_speed       real,
    fr_wheel_speed       real,
    steer_col_angle      real,
    bl_gyro              real[],
    bl_sprung_accel      real[],
    br_gyro              real[],
    br_sprung_accel      real[],
    fl_gyro              real[],
    fl_sprung_accel      real[],
    fr_gyro              real[],
    fr_sprung_accel      real[],
    bl_ride_height       real,
    bl_strain_gauge_v    real,
    bl_sus_pot_v         real,
    br_ride_height       real,
    br_strain_gauge_v    real,
    br_sus_pot_v         real,
    fl_ride_height       real,
    fl_strain_gauge_v    real,
    fl_sus_pot_v         real,
    fr_ride_height       real,
    fr_strain_gauge_v    real,
    fr_sus_pot_v         real,
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
    line_lock_enabled    boolean,
    torque_shudder       real,
    CONSTRAINT fk_controls_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Pack Table
CREATE TABLE public.pack (
    packet_id           bigint   NOT NULL,
    bus_voltage          real,
    lv_boards_current    real,
    soc_estimate         real,
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
    prndl_state          real,
    shutdown_current     real,
    hvc_state_machine    real,
    post_faults          real,
    run_faults           real,
    apps1_disconnect     boolean,
    apps1_out_range      boolean,
    apps2_disconnect     boolean,
    apps2_out_range      boolean,
    apps_implause        boolean,
    apps_mismatch        boolean,
    batt_fans_fuse       boolean,
    batt_pump_fuse       boolean,
    boards_fuse          boolean,
    bpps1_disconnect     boolean,
    bpps1_out_range      boolean,
    bpps2_disconnect     boolean,
    bpps2_out_range      boolean,
    bpps_mismatch        boolean,
    brake_light_fuse     boolean,
    bse1_disconnect      boolean,
    bse1_out_range       boolean,
    bse2_disconnect      boolean,
    bse2_out_range       boolean,
    ll_fuse              boolean,
    motor_pump_fuse      boolean,
    r2d_buzzer           boolean,
    rtd_fuse             boolean,
    shtdn_fuse           boolean,
    shutdown_bspd_status boolean,
    shutdown_emeter_status boolean,
    spare_fuse           boolean,
    stomp_fault          boolean,
    tssi_green_fuse      boolean,
    tssi_red_fuse        boolean,
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
    temp_imd_1           boolean,
    temp_imd_2           boolean,
    temp_shutdown_1      boolean,
    temp_shutdown_2      boolean,
    CONSTRAINT fk_diagnostics_low_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Thermal Table
CREATE TABLE public.thermal (
    packet_id           bigint   NOT NULL,
    batt_cooling_current real,
    motor_cooling_current real,
    ambient_temp         real,
    motor_temp           real,
    batt_loop_batt_temp  real,
    batt_loop_rad_fan_speed real,
    batt_loop_rad_temp   real,
    battery_fan_rpm      real,
    bus_bar_temp1        real,
    bus_bar_temp2        real,
    bus_bar_temp3        real,
    cell_bottom_temp     real,
    cell_top_temp        real,
    coolant_flow_lpm     real,
    coolant_temp         real,
    discharge_r_temp     real,
    fan_rpm              real,
    gate_driver_temp     real,
    inverter_hotspot_temp real,
    inverter_temp        real,
    max_cell_voltage     real,
    min_cell_voltage     real,
    module_a_temp        real,
    module_b_temp        real,
    module_c_temp        real,
    motor_loop_inverter_temp real,
    motor_loop_motor_temp real,
    motor_loop_rad_temp  real,
    temp_ams_1           boolean,
    temp_ams_2           boolean,
    temp_command_1       boolean,
    temp_command_2       boolean,
    temp_output_1        boolean,
    temp_output_2        boolean,
    CONSTRAINT fk_thermal_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);

-- Generated Board_status Table
CREATE TABLE public.board_status (
    packet_id           bigint   NOT NULL,
    csm_last_seen_s      real,
    dui_last_seen_s      real,
    hvc_last_seen_s      real,
    inverter_last_seen_s real,
    pdu_last_seen_s      real,
    tsm_last_seen_s      real,
    usm_last_seen_s      real,
    vcu_last_seen_s      real,
    CONSTRAINT fk_board_status_packet_id FOREIGN KEY(packet_id) REFERENCES packet(packet_id)
);
