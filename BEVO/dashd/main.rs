use anyhow::Result;
use prost::Message;
use rumqttc::{Client, Event, Incoming, MqttOptions, QoS};
use serde::Serialize;
use std::io::Read;
use std::net::TcpListener;
use std::os::unix::net::UnixStream;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use sensor_proto::proto::orion::OrionSensorData;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const WS_PORT: u16 = 8001;
const WS_SEND_HZ: u64 = 30;

const MQTT_HOST: &str = "18.191.225.118";
const MQTT_PORT: u16 = 1883;
const MQTT_CLIENT_ID: &str = "BEVO-DASHD";
const MQTT_TOPIC_PREFIX: &str = "lhre/dash/";
const MQTT_STALE_TIMEOUT: Duration = Duration::from_secs(5);

/// If we haven't gotten a fresh OrionSensorData snapshot from cand within
/// this window, the next WebSocket frame ships an all-default CanData so
/// the dash shows "--" instead of frozen last-known values.
const CAN_STALE_TIMEOUT: Duration = Duration::from_secs(3);

/// Largest gap between CAN frames we'll integrate energy across. A longer gap
/// (cand stall, IPC hiccup) is treated as lost time rather than dumping a slug
/// of phantom Wh into the lap total.
const MAX_ENERGY_DT_S: f64 = 0.5;

/// How often dashd publishes its driver-facing state to `lhre/dash/state` for
/// the trackside mirror / link-health panel.
const STATE_PUBLISH_HZ: u64 = 2;

fn env_or_default(name: &str, default: &str) -> String {
    std::env::var(name).unwrap_or_else(|_| default.to_string())
}

// ---------------------------------------------------------------------------
// JSON schema structs — must match DashData.ts exactly
// ---------------------------------------------------------------------------

#[derive(Serialize, Clone, Default)]
struct CanData {
    /// Vehicle speed in MPH derived from motor RPM via drivetrain geometry:
    /// `motor_speed * 2*pi/60 * 13/43 * 0.201 * 2.237` (rpm→rad/s, motor→wheel
    /// gear ratio, wheel radius m, m/s→mph). Source: controls.motor_speed.
    speed: Option<f32>,

    /// Electrical power in kW, derived as dc_bus_v * dc_bus_current / 1000.
    power: Option<f32>,

    /// NOT AVAILABLE — no CAN field for odometer.
    /// Would require integrating wheel_speed over time, and speed unit must be
    /// confirmed first. Always null.
    odometer: Option<f32>,

    /// Estimated SOC from the inverter's DC bus voltage:
    /// `(dc_bus_v - 390) / (546 / 390)`. Only refreshed while the bus
    /// current magnitude is below 1.0 A (so we're sampling cell EMF, not
    /// IR drop). Held at the last qualified value otherwise. Used because
    /// HVC firmware does not currently broadcast packet 0x132 carrying
    /// pack.hv_soc; once 0x132 is back, prefer that.
    soc: Option<f32>,

    /// Pack temperature in °C — max of all reported cell temps
    /// (pack.cells_temps from packet 0x100). Was thermal.cell_top_temp,
    /// but that field arrives in packet 0x132 which HVC does not
    /// currently emit. Same value as `cellTempMax` below.
    temperature: Option<f32>,

    /// NOT AVAILABLE — 5G signal strength is not on the CAN bus. Always null.
    /// Could be sourced from the cellular modem (see BEVO/cell.py) in a
    /// follow-up.
    #[serde(rename = "signalStrength")]
    signal_strength: Option<f32>,

    /// 4-leg shutdown circuit state from diagnostics_low.shutdown_legX.
    /// Emitted as [leg1, leg2, leg3, leg4]; frontend renders a "FAULT" if any
    /// element is false. The 16-name mapping originally expected by the
    /// frontend has been collapsed to these four legs in SHUTDOWN_NAMES.
    shutdown: Option<Vec<bool>>,

    /// PRNDL state from diagnostics_high.prndl_state. "P" for PARK (0),
    /// "D" for DRIVE (1), None when no diag data or unknown value. Other
    /// gears (R/N/L) aren't implemented in VCU firmware; matches the
    /// VCU enum in VCU/model/components/PRNDL.h.
    prndl: Option<&'static str>,

    /// HV contactor states from HVC packet 0x131 (Contactor Status) via
    /// diagnostics_high.{pos_hv_contactor, neg_hv_contactor,
    /// precharge_contactor}. Frontend combines pos+neg into a single
    /// "HV UP/DOWN" indicator; precharge is exposed separately for pit
    /// diagnostics use.
    #[serde(rename = "posContactor")]
    pos_contactor: Option<bool>,
    #[serde(rename = "negContactor")]
    neg_contactor: Option<bool>,
    #[serde(rename = "prechargeContactor")]
    precharge_contactor: Option<bool>,

    // ---------------------------------------------------------------
    // Pit / extended driver-thread fields — all sourced from the same
    // OrionSensorData snapshot cand publishes. None when cand has not
    // received the corresponding CAN message yet.
    // ---------------------------------------------------------------

    /// Front brake-bias percentage (controls.brake_bias).
    #[serde(rename = "brakeBias")]
    brake_bias: Option<f32>,

    /// Accelerator pedal travel %, controls.apps1_travel.
    apps: Option<f32>,
    /// Brake pedal travel %, controls.bpps1_travel.
    bpps: Option<f32>,

    /// Front brake pressure (controls.brake_pressure_f).
    #[serde(rename = "brakePressureFront")]
    brake_pressure_front: Option<f32>,
    /// Rear brake pressure — sum of the two rear sensors (rall + rbll).
    #[serde(rename = "brakePressureRear")]
    brake_pressure_rear: Option<f32>,

    /// Motor temperature °C (thermal.motor_temp).
    #[serde(rename = "motorTemp")]
    motor_temp: Option<f32>,
    /// Inverter temperature °C (thermal.inverter_temp).
    #[serde(rename = "inverterTemp")]
    inverter_temp: Option<f32>,
    /// Coolant temperature °C (thermal.coolant_temp).
    #[serde(rename = "coolantTemp")]
    coolant_temp: Option<f32>,

    /// Pack-wide cell temp aggregates from pack.cells_temps[] (°C).
    /// None when the array is empty (cand has not yet seen a cell-temp packet).
    #[serde(rename = "cellTempMax")]
    cell_temp_max: Option<f32>,
    #[serde(rename = "cellTempAvg")]
    cell_temp_avg: Option<f32>,
    #[serde(rename = "cellTempMin")]
    cell_temp_min: Option<f32>,

    /// HV pack voltage (pack.hv_pack_v).
    #[serde(rename = "hvVoltage")]
    hv_voltage: Option<f32>,
    /// HV pack current (pack.hv_c).
    #[serde(rename = "hvCurrent")]
    hv_current: Option<f32>,
    /// GLV / LV bus voltage (pack.lv_batt_v).
    #[serde(rename = "lvVoltage")]
    lv_voltage: Option<f32>,
    /// GLV / LV bus current (pack.lv_batt_c).
    #[serde(rename = "lvCurrent")]
    lv_current: Option<f32>,

    /// Per-wheel speed in same units as `wheel_speed` (dynamics.fl/fr/bl/br_wheel_speed).
    #[serde(rename = "wheelSpeedFL")]
    wheel_speed_fl: Option<f32>,
    #[serde(rename = "wheelSpeedFR")]
    wheel_speed_fr: Option<f32>,
    #[serde(rename = "wheelSpeedRL")]
    wheel_speed_rl: Option<f32>,
    #[serde(rename = "wheelSpeedRR")]
    wheel_speed_rr: Option<f32>,

    /// Regen-armed state. Wire source is byte 5 of packet 0x1C7, which
    /// the CSV labels `line_lock_enabled` but actually carries the
    /// regen-enabled bit per the VCU team. Stored under the field's
    /// CSV name in the proto; renamed here for the frontend's
    /// `regenEnabled` pill.
    #[serde(rename = "regenEnabled")]
    regen_enabled: Option<bool>,

    /// Active VCU event mode (which params table the VCU is running).
    /// 0 = unassigned, 1 = acceleration, 2 = skidpad, 3 = autocross, 4 = endurance.
    /// Lives on Controls.event_mode (byte 6 of 0x1C7 VCU State).
    #[serde(rename = "eventMode")]
    event_mode: Option<u8>,

    /// Pack cell-voltage aggregates from pack.cells_v[] (V). None until cand
    /// has received a cell-voltage packet (empty vec). `spread` (max - min) is
    /// the pit crew's pack-imbalance health check — a widening spread flags a
    /// weak/failing cell before it becomes a DNF.
    #[serde(rename = "cellVMax")]
    cell_v_max: Option<f32>,
    #[serde(rename = "cellVMin")]
    cell_v_min: Option<f32>,
    #[serde(rename = "cellVSpread")]
    cell_v_spread: Option<f32>,

    /// Running cumulative energy from the VCU's 0x1C9 Energy Estimate
    /// (Controls.net_energy / regen_energy, Wh). `net` is drive-minus-regen
    /// returned; `regen` is cumulative regen. The VCU is the source of truth
    /// for energy (no client-side integration) — these are what the pit crew
    /// watches in Park to read session usage.
    #[serde(rename = "vcuNetEnergyWh")]
    vcu_net_energy_wh: Option<f32>,
    #[serde(rename = "vcuRegenEnergyWh")]
    vcu_regen_energy_wh: Option<f32>,
}

#[derive(Serialize, Clone, Default)]
struct MqttData {
    #[serde(rename = "lapDelta")]
    lap_delta: Option<f32>,

    #[serde(rename = "energyDelta")]
    energy_delta: Option<f32>,

    #[serde(rename = "lapsRemaining")]
    laps_remaining: Option<f32>,

    /// Live target power budget (kW) entered by the trackside team. The dash
    /// paces per-lap energy against this: expected_Wh = targetPower * elapsed.
    /// Held last-known across dropouts (NOT nulled on staleness) so a cellular
    /// blip doesn't blank the driver's pacing reference — see target_power_stale.
    #[serde(rename = "targetPower")]
    target_power: Option<f32>,

    /// True when the held targetPower is older than the staleness window, so
    /// the frontend can dim it / show a "STALE" badge instead of pretending
    /// it's live. None when there's no targetPower at all.
    #[serde(rename = "targetPowerStale")]
    target_power_stale: Option<bool>,

    /// Monotonic lap counter (DashState.lap_count). Bumped by the on-car GPS
    /// start/finish detector OR a trackside lapTrigger, whichever fires. The
    /// frontend pops the lap card + resets the per-lap integrator on each
    /// increase. Always emitted fresh — it's local state, not a network value.
    #[serde(rename = "lapTrigger")]
    lap_trigger: Option<f32>,

    /// Website-set duration (ms) the full-screen lap card stays up after a lap.
    /// Held last-known (it's a config value, not a live signal). None until the
    /// trackside team sets it → frontend uses its built-in default.
    #[serde(rename = "lapCardMs")]
    lap_card_ms: Option<f32>,
}

/// Endurance pacing, computed authoritatively on-car so the driver and the
/// trackside mirror read identical numbers, and so the lap integrator survives
/// a chromium reload (it lives here, not in the frontend).
#[derive(Serialize, Clone, Default)]
#[serde(rename_all = "camelCase")]
struct PacingData {
    /// Net energy used so far this lap (drive minus regen), Wh.
    lap_energy_wh: f32,
    /// used - budget for this lap, Wh. Positive = over budget (red),
    /// negative = banking margin (green). None until a targetPower is set.
    budget_delta_wh: Option<f32>,
    /// Wall-clock seconds since the current lap started.
    lap_elapsed_s: f32,
    /// 1-based lap currently in progress.
    lap_number: u32,
    /// Most recently completed lap (drives the full-screen lap card).
    last_lap_number: Option<u32>,
    last_lap_time_s: Option<f32>,
    last_lap_energy_wh: Option<f32>,
}

#[derive(Serialize, Clone)]
struct DashMessage {
    seq: u64,
    can: CanData,
    mqtt: MqttData,
    pacing: PacingData,
    /// Website-authored lap-card layout (retained `lhre/dash/layout`), forwarded
    /// verbatim to the frontend. None until one is sent → frontend uses its
    /// built-in lap card.
    #[serde(skip_serializing_if = "Option::is_none")]
    layout: Option<serde_json::Value>,
    /// Website-authored park/pit-screen layout (retained `lhre/dash/parkLayout`).
    /// None until one is sent → frontend uses its built-in park screen.
    #[serde(rename = "parkLayout", skip_serializing_if = "Option::is_none")]
    park_layout: Option<serde_json::Value>,
}

/// Snapshot published to `lhre/dash/state` for the trackside link-health /
/// dash-mirror panel — the same numbers the driver sees, plus the controls the
/// car actually received (so trackside can confirm uplink, not just guess).
#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct DashStateMsg {
    lap_count: u64,
    target_power: Option<f32>,
    target_power_stale: bool,
    speed: Option<f32>,
    power: Option<f32>,
    soc: Option<f32>,
    temperature: Option<f32>,
    pacing: PacingData,
    /// Free + total space on `/` (MB). The car's storage kill switch
    /// (start_telemetry.sh) stops telemetry when free < 1024 MB, so trackside
    /// can warn before that. None until the first disk poll.
    disk_free_mb: Option<f64>,
    disk_total_mb: Option<f64>,
    /// Seconds the telemetry stack has been running. The same kill switch stops
    /// at 3600 s (1 h); surfaced so trackside can watch the runtime cap too.
    runtime_s: Option<f32>,
}

// ---------------------------------------------------------------------------
// Shared state between all threads
// ---------------------------------------------------------------------------

struct MqttState {
    lap_delta: Option<f32>,
    energy_delta: Option<f32>,
    laps_remaining: Option<f32>,
    target_power: Option<f32>,
    lap_card_ms: Option<f32>,
    last_lap_delta: Instant,
    last_energy_delta: Instant,
    last_laps_remaining: Instant,
    last_target_power: Instant,
}

impl MqttState {
    fn new() -> Self {
        let epoch = Instant::now() - MQTT_STALE_TIMEOUT; // start stale
        Self {
            lap_delta: None,
            energy_delta: None,
            laps_remaining: None,
            target_power: None,
            lap_card_ms: None,
            last_lap_delta: epoch,
            last_energy_delta: epoch,
            last_laps_remaining: epoch,
            last_target_power: epoch,
        }
    }

    /// Convert to the JSON-serializable MqttData. Most fields null out once
    /// stale; targetPower is the exception — it's held last-known (with a
    /// staleness flag) so a dropout doesn't strip the driver's pacing
    /// reference. lap_trigger is filled in by the WS sender from lap_count.
    fn to_mqtt_data(&self) -> MqttData {
        let now = Instant::now();
        let target_power_age = now.duration_since(self.last_target_power);
        MqttData {
            lap_delta: self
                .lap_delta
                .filter(|_| now.duration_since(self.last_lap_delta) < MQTT_STALE_TIMEOUT),
            energy_delta: self
                .energy_delta
                .filter(|_| now.duration_since(self.last_energy_delta) < MQTT_STALE_TIMEOUT),
            laps_remaining: self
                .laps_remaining
                .filter(|_| now.duration_since(self.last_laps_remaining) < MQTT_STALE_TIMEOUT),
            // Hold last-known; never null on staleness.
            target_power: self.target_power,
            target_power_stale: self
                .target_power
                .map(|_| target_power_age >= MQTT_STALE_TIMEOUT),
            // Overwritten by the WS sender with lap_count.
            lap_trigger: None,
            // Config value — held last-known, never nulled on staleness.
            lap_card_ms: self.lap_card_ms,
        }
    }
}

struct DashState {
    can: CanData,
    /// When this dashd (i.e. the telemetry stack) started — for the runtime
    /// readout vs the 1-hour storage kill switch.
    boot: Instant,
    /// Latest free / total space on `/` (MB), refreshed by disk_monitor_loop.
    disk_free_mb: Option<f64>,
    disk_total_mb: Option<f64>,
    /// Wall-clock time of the most recent successful IPC decode. Used by the
    /// WS sender to null out CanData once cand has stopped publishing.
    last_can_update: Instant,
    /// Most recent SOC estimate that satisfied the current-qualification
    /// gate (|dc_bus_current| < 1.0 A). Reused while the gate is open so
    /// the bar doesn't twitch under load. None until first qualified read.
    last_qualified_soc: Option<f32>,
    mqtt: MqttState,

    // ---- Endurance lap counting (link-independent) ----
    /// Start/finish line as [lat1, lon1, lat2, lon2]. Loaded from the retained
    /// `lhre/dash/sfGate` topic (and disk on boot). When present, the car
    /// counts its own laps from GPS — no per-lap network dependency.
    sf_gate: Option<[f64; 4]>,
    /// Previous GPS fix (lat, lon), for segment-crossing detection.
    last_gps: Option<(f64, f64)>,
    /// Monotonic lap count — bumped by a GPS crossing or a trackside lapTrigger.
    /// Emitted to the frontend as mqtt.lapTrigger.
    lap_count: u64,
    /// Last trackside lapTrigger value seen, for rising-edge detection.
    last_offcar_trigger: Option<f32>,

    // ---- On-car energy integration (authoritative) ----
    /// Net energy used this lap (Wh), integrated from CAN power. Reset each lap.
    lap_energy_wh: f64,
    /// Energy budget consumed this lap at targetPower (Wh). Reset each lap.
    lap_budget_wh: f64,
    /// Start of the current lap, for elapsed time.
    lap_start: Instant,
    /// Previous integration tick, for dt. None until the first frame.
    last_energy_update: Option<Instant>,
    /// Snapshot of the most recently completed lap (for the lap card / mirror).
    last_lap: Option<(u32, f32, f32)>, // (lap_number, time_s, energy_wh)

    /// Website-authored lap-card layout (retained `lhre/dash/layout`), held as
    /// raw JSON and forwarded to the frontend. None → frontend's built-in card.
    layout: Option<serde_json::Value>,
    /// Website-authored park/pit-screen layout (retained `lhre/dash/parkLayout`).
    /// None → frontend's built-in park screen.
    park_layout: Option<serde_json::Value>,
}

impl DashState {
    /// Close the in-progress lap: snapshot its time + energy for the card,
    /// set the counter, and reset the per-lap integrators. Called from both
    /// lap sources (on-car GPS crossing, trackside lapTrigger) so the energy
    /// reset can never drift from the lap boundary.
    ///
    /// `set_count`: if Some, ADOPT that as the new lap_count (trackside is the
    /// source of truth — the website's `liveState.laps.length` always wins so
    /// the dash and the website display the same number). If None, bump by 1
    /// (the GPS path doesn't know the absolute number, just that one closed).
    fn complete_lap(&mut self, now: Instant, set_count: Option<u64>) {
        let next_count = set_count.unwrap_or(self.lap_count + 1);
        self.last_lap = Some((
            next_count as u32,
            now.duration_since(self.lap_start).as_secs_f32(),
            self.lap_energy_wh as f32,
        ));
        self.lap_count = next_count;
        self.lap_energy_wh = 0.0;
        self.lap_budget_wh = 0.0;
        self.lap_start = now;
    }

    /// Build the pacing snapshot shown to the driver and mirrored to trackside.
    fn pacing(&self, now: Instant) -> PacingData {
        let (last_n, last_t, last_e) = match self.last_lap {
            Some((n, t, e)) => (Some(n), Some(t), Some(e)),
            None => (None, None, None),
        };
        PacingData {
            lap_energy_wh: self.lap_energy_wh as f32,
            // Only meaningful once a target has been set this lap.
            budget_delta_wh: self
                .mqtt
                .target_power
                .map(|_| (self.lap_energy_wh - self.lap_budget_wh) as f32),
            lap_elapsed_s: now.duration_since(self.lap_start).as_secs_f32(),
            lap_number: (self.lap_count + 1) as u32,
            last_lap_number: last_n,
            last_lap_time_s: last_t,
            last_lap_energy_wh: last_e,
        }
    }
}

// Path the loaded start/finish gate is cached to, so it survives a dashd or
// car reboot even if the broker drops its retained copy. Override with
// DASHD_SFGATE_PATH.
const SFGATE_PATH: &str = "/tmp/BEVO_dash_sfgate.json";

// Path the website-authored lap-card layout is cached to (retained on
// `lhre/dash/layout`), so it survives a dashd/car reboot. Override with
// DASHD_LAYOUT_PATH.
const LAYOUT_PATH: &str = "/tmp/BEVO_dash_layout.json";

/// Does the car's path segment (prev->cur GPS) cross the gate line? Both car
/// points are (lat, lon); the gate is [lat1, lon1, lat2, lon2]. Lat/lon are
/// treated as a planar (x=lon, y=lat) frame — fine over a ~10 m gate. Returns
/// true only on a proper crossing (segments strictly intersect).
fn segments_cross(prev: (f64, f64), cur: (f64, f64), gate: [f64; 4]) -> bool {
    // To (x=lon, y=lat).
    let a = (prev.1, prev.0);
    let b = (cur.1, cur.0);
    let c = (gate[1], gate[0]);
    let d = (gate[3], gate[2]);
    // Signed area of triangle (p, q, r): >0 / <0 = r left / right of line pq.
    let orient = |p: (f64, f64), q: (f64, f64), r: (f64, f64)| -> f64 {
        (q.0 - p.0) * (r.1 - p.1) - (q.1 - p.1) * (r.0 - p.0)
    };
    let d1 = orient(c, d, a); // car-prev vs gate line
    let d2 = orient(c, d, b); // car-cur  vs gate line
    let d3 = orient(a, b, c); // gate-1   vs car path
    let d4 = orient(a, b, d); // gate-2   vs car path
    ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0))
        && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0))
}

fn sfgate_path() -> String {
    env_or_default("DASHD_SFGATE_PATH", SFGATE_PATH)
}

/// Cache the gate to disk so it survives a dashd/car reboot even if the broker
/// drops its retained copy (mosquitto without persistence).
fn persist_sf_gate(gate: &[f64; 4]) {
    match serde_json::to_string(gate) {
        Ok(json) => {
            if let Err(e) = std::fs::write(sfgate_path(), json) {
                eprintln!("[DASHD] Failed to persist sfGate: {}", e);
            }
        }
        Err(e) => eprintln!("[DASHD] Failed to serialize sfGate: {}", e),
    }
}

/// Restore the last-known gate on boot, before the broker (re)delivers it.
fn load_sf_gate() -> Option<[f64; 4]> {
    let path = sfgate_path();
    let data = std::fs::read_to_string(&path).ok()?;
    match serde_json::from_str::<[f64; 4]>(data.trim()) {
        Ok(gate) => {
            println!("[DASHD] Restored start/finish gate from {}: {:?}", path, gate);
            Some(gate)
        }
        Err(_) => None,
    }
}

fn layout_path() -> String {
    env_or_default("DASHD_LAYOUT_PATH", LAYOUT_PATH)
}

/// Cache the lap-card layout to disk so it survives a dashd/car reboot even if
/// the broker drops its retained copy.
fn persist_layout(raw: &str) {
    if let Err(e) = std::fs::write(layout_path(), raw) {
        eprintln!("[DASHD] Failed to persist layout: {}", e);
    }
}

/// Restore the last-known layout on boot, before the broker (re)delivers it.
fn load_layout() -> Option<serde_json::Value> {
    let path = layout_path();
    let data = std::fs::read_to_string(&path).ok()?;
    match serde_json::from_str::<serde_json::Value>(data.trim()) {
        Ok(v) => {
            println!("[DASHD] Restored lap-card layout from {}", path);
            Some(v)
        }
        Err(_) => None,
    }
}

// Park/pit-screen layout — same disk-cache pattern as the lap-card layout, own
// file + env override so the two survive a reboot independently.
const PARK_LAYOUT_PATH: &str = "/tmp/BEVO_dash_parklayout.json";

fn park_layout_path() -> String {
    env_or_default("DASHD_PARK_LAYOUT_PATH", PARK_LAYOUT_PATH)
}

fn persist_park_layout(raw: &str) {
    if let Err(e) = std::fs::write(park_layout_path(), raw) {
        eprintln!("[DASHD] Failed to persist park layout: {}", e);
    }
}

fn load_park_layout() -> Option<serde_json::Value> {
    let path = park_layout_path();
    let data = std::fs::read_to_string(&path).ok()?;
    match serde_json::from_str::<serde_json::Value>(data.trim()) {
        Ok(v) => {
            println!("[DASHD] Restored park layout from {}", path);
            Some(v)
        }
        Err(_) => None,
    }
}

// ---------------------------------------------------------------------------
// Protobuf -> CanData extraction
// ---------------------------------------------------------------------------

fn extract_can_data(data: &OrionSensorData, last_qualified_soc: &mut Option<f32>) -> CanData {
    let pack = data.pack.as_ref();
    let thermal = data.thermal.as_ref();
    let controls = data.controls.as_ref();
    let dynamics = data.dynamics.as_ref();
    let diag_low = data.diagnostics_low.as_ref();
    let diag_high = data.diagnostics_high.as_ref();

    // Vehicle speed from motor RPM (inverter feedback).
    const MOTOR_RPM_TO_MPH: f32 = 0.014233265;
    let speed = controls.map(|c| c.motor_speed * MOTOR_RPM_TO_MPH);

    let power = pack.map(|p| p.dc_bus_v * p.dc_bus_current / 1000.0);

    // Update the dc-bus-derived fallback (kept for when VCU isn't
    // broadcasting yet or reads 0). Same qualification + floor as before.
    if let Some(p) = pack {
        if p.dc_bus_current.abs() < 1.0 {
            let raw = (p.dc_bus_v - 390.0) / (546.0 / 390.0);
            *last_qualified_soc = Some(raw.max(0.0));
        }
    }

    // VCU-only SOC for now. Per driveday request: dc_bus fallback is
    // commented out so the dash shows exactly what VCU broadcasts on
    // packet 0x1C7 (mm/vcu-soc). To restore the fallback, swap to:
    //   let soc = pack
    //       .and_then(|p| if p.soc_estimate > 0.0 { Some(p.soc_estimate) } else { None })
    //       .or(*last_qualified_soc);
    // NOTE: soc_estimate lives on the Pack message (not DiagnosticsHigh).
    let soc = pack.map(|p| p.soc_estimate);

    // Per-cell temp aggregates. Like cells_v[], cand fills cells_temps[] as
    // packets arrive, so not-yet-received slots sit at exactly 0.0 — exclude
    // those so min/avg aren't dragged toward 0 by unpopulated cells (max was
    // already correct, which is why the driving screen's TEMP gauge looked
    // fine). We filter *exactly* 0.0 rather than `> 0.0` because a real cell
    // can read sub-zero. None until at least one populated cell is seen.
    let (cell_temp_max, cell_temp_avg, cell_temp_min) = pack
        .map(|p| {
            let vals: Vec<f32> = p.cells_temps.iter().cloned().filter(|&t| t != 0.0).collect();
            if vals.is_empty() {
                (None, None, None)
            } else {
                let max = vals.iter().cloned().fold(f32::NEG_INFINITY, f32::max);
                let min = vals.iter().cloned().fold(f32::INFINITY, f32::min);
                let avg = vals.iter().sum::<f32>() / vals.len() as f32;
                (Some(max), Some(avg), Some(min))
            }
        })
        .unwrap_or((None, None, None));

    // Per-cell voltage aggregates from pack.cells_v[]. spread = max - min, the
    // pack-imbalance metric. cand fills cells_v[] as per-cell packets arrive, so
    // not-yet-received slots sit at exactly 0.0 — exclude those (a connected
    // cell is never 0 V) or a half-populated array drags min/spread to garbage.
    // None until at least one real (>0 V) cell has been seen. NB: cell *temps*
    // are NOT filtered this way — 0 °C is a plausible real reading.
    let (cell_v_max, cell_v_min, cell_v_spread) = pack
        .map(|p| {
            let vals: Vec<f32> = p.cells_v.iter().cloned().filter(|&v| v > 0.0).collect();
            if vals.is_empty() {
                (None, None, None)
            } else {
                let max = vals.iter().cloned().fold(f32::NEG_INFINITY, f32::max);
                let min = vals.iter().cloned().fold(f32::INFINITY, f32::min);
                (Some(max), Some(min), Some(max - min))
            }
        })
        .unwrap_or((None, None, None));

    // Pack temp = hottest cell. Bound to cell_temp_max above; same source.
    let temperature = cell_temp_max;

    // Shutdown array — packed in the order SHUTDOWN_NAMES expects.
    // Convention: true = OK, false = FAULT. Source fields are normalized to
    // that convention here:
    //   - shutdown_legN / shutdown_*_status: wire bit = 1 means leg closed
    //   - *_error / temp_shutdown_*: wire bit = 1 means a fault, so inverted
    let shutdown = match (diag_low, diag_high) {
        (Some(low), Some(high)) => Some(vec![
            low.shutdown_leg1,
            low.shutdown_leg2,
            low.shutdown_leg3,
            low.shutdown_leg4,
            !low.bmb_comm_error,
            !low.imd_gnd_isolation_error,
            high.shutdown_bspd_status,
            high.shutdown_emeter_status,
            !low.temp_shutdown_1,
            !low.temp_shutdown_2,
        ]),
        (Some(low), None) => Some(vec![
            low.shutdown_leg1,
            low.shutdown_leg2,
            low.shutdown_leg3,
            low.shutdown_leg4,
            !low.bmb_comm_error,
            !low.imd_gnd_isolation_error,
            false,
            false,
            !low.temp_shutdown_1,
            !low.temp_shutdown_2,
        ]),
        _ => None,
    };

    // PRNDL: float on the wire but VCU's enum only emits 0 (PARK) or
    // 1 (DRIVE) — see VCU/model/components/PRNDL.h. Anything else is
    // unexpected, so map to None.
    let prndl: Option<&'static str> = diag_high.and_then(|d| match d.prndl_state as i32 {
        0 => Some("P"),
        1 => Some("D"),
        _ => None,
    });

    CanData {
        speed,
        power,
        soc,
        temperature,
        odometer: None,
        signal_strength: None,
        shutdown,
        prndl,
        pos_contactor: diag_high.map(|d| d.pos_hv_contactor),
        neg_contactor: diag_high.map(|d| d.neg_hv_contactor),
        precharge_contactor: diag_high.map(|d| d.precharge_contactor),

        // Workaround: the can.json precision on brake_bias is 0.01, so a
        // raw VCU byte of 45 (meaning 45%) decodes to 0.45 and the dash
        // rounds to "0%". Multiply back to 0..100 here. Proper fix is in
        // longhorn-lib's can_packets.csv (set precision to 1.0); remove
        // this scaling once that lands and can.json refreshes.
        brake_bias: controls.map(|c| c.brake_bias * 100.0),
        apps: controls.map(|c| c.apps1_travel),
        bpps: controls.map(|c| c.bpps1_travel),
        brake_pressure_front: controls.map(|c| c.brake_pressure_f),
        // rall + rbll are two rear pressure sensors. Averaged here so the
        // display reads a single "rear pressure" regardless of which sensor
        // (or both, redundantly) is reporting. Worth revisiting once the
        // brake team confirms whether these are redundant or L/R-split.
        brake_pressure_rear: controls
            .map(|c| (c.brake_pressure_rall + c.brake_pressure_rbll) / 2.0),

        motor_temp: thermal.map(|t| t.motor_temp),
        inverter_temp: thermal.map(|t| t.inverter_temp),
        coolant_temp: thermal.map(|t| t.coolant_temp),
        cell_temp_max,
        cell_temp_avg,
        cell_temp_min,

        hv_voltage: pack.map(|p| p.hv_pack_v),
        hv_current: pack.map(|p| p.hv_c),
        lv_voltage: pack.map(|p| p.lv_batt_v),
        lv_current: pack.map(|p| p.lv_batt_c),

        wheel_speed_fl: dynamics.map(|d| d.fl_wheel_speed),
        wheel_speed_fr: dynamics.map(|d| d.fr_wheel_speed),
        wheel_speed_rl: dynamics.map(|d| d.bl_wheel_speed),
        wheel_speed_rr: dynamics.map(|d| d.br_wheel_speed),

        // CSV byte 5 is named `line_lock_enabled` but the VCU team
        // confirmed it carries the regen-enabled bit. Plumb to the
        // frontend's regenEnabled pill. Lives on the Controls message.
        regen_enabled: controls.map(|c| c.line_lock_enabled),

        // Wire value is uint8 but the proto declares the field as float
        // (matches prndl/soc convention); cast back for the frontend enum.
        event_mode: controls.map(|c| c.event_mode as u8),

        cell_v_max,
        cell_v_min,
        cell_v_spread,

        // Running cumulative energy from the VCU (Controls.net_energy /
        // regen_energy, 0x1C9). Shown on the Park debug screen.
        vcu_net_energy_wh: controls.map(|c| c.net_energy),
        vcu_regen_energy_wh: controls.map(|c| c.regen_energy),
    }
}

// ---------------------------------------------------------------------------
// Thread 1: IPC reader — connects to cand Unix socket, updates shared state
// ---------------------------------------------------------------------------

fn ipc_reader_loop(state: Arc<Mutex<DashState>>) {
    loop {
        println!("[DASHD] Connecting to {}...", SOCKET_PATH);

        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                println!("[DASHD] Connected to cand IPC");
                loop {
                    // Read 4-byte big-endian length prefix
                    let mut len_buf = [0u8; 4];
                    if stream.read_exact(&mut len_buf).is_err() {
                        println!("[DASHD] cand disconnected, will reconnect");
                        break;
                    }

                    let msg_len = u32::from_be_bytes(len_buf) as usize;
                    let mut msg_buf = vec![0u8; msg_len];
                    if let Err(e) = stream.read_exact(&mut msg_buf) {
                        eprintln!("[DASHD] Read error: {}, will reconnect", e);
                        break;
                    }

                    match OrionSensorData::decode(&msg_buf[..]) {
                        Ok(data) => {
                            // GPS comes in dynamics.gps as [lat, lon] (cand NMEA).
                            let gps = data
                                .dynamics
                                .as_ref()
                                .filter(|d| d.gps.len() >= 2)
                                .map(|d| (d.gps[0] as f64, d.gps[1] as f64))
                                // (0,0) is the no-fix sentinel — ignore it.
                                .filter(|&(lat, lon)| lat != 0.0 || lon != 0.0);

                            let now = Instant::now();
                            let mut locked = state.lock().unwrap();
                            let can_data = extract_can_data(&data, &mut locked.last_qualified_soc);
                            let power = can_data.power;
                            locked.can = can_data;
                            locked.last_can_update = now;

                            // Integrate energy for this lap: Wh += kW * dt(s) / 3.6.
                            // Regen (negative power) credits back. Budget tracks
                            // targetPower over the same steps so the comparison is
                            // apples-to-apples. Clamp dt so a stall can't dump a
                            // phantom slug of energy.
                            if let Some(prev) = locked.last_energy_update {
                                let dt = now.duration_since(prev).as_secs_f64().min(MAX_ENERGY_DT_S);
                                if dt > 0.0 {
                                    if let Some(p) = power {
                                        locked.lap_energy_wh += (p as f64) * dt / 3.6;
                                        if let Some(tp) = locked.mqtt.target_power {
                                            locked.lap_budget_wh += (tp as f64) * dt / 3.6;
                                        }
                                    }
                                }
                            }
                            locked.last_energy_update = Some(now);

                            // On-car lap detection: close the lap when the path
                            // between consecutive fixes crosses the gate.
                            if let Some(cur) = gps {
                                if let (Some(gate), Some(prev)) = (locked.sf_gate, locked.last_gps) {
                                    if segments_cross(prev, cur, gate) {
                                        locked.complete_lap(now, None);
                                        println!("[DASHD] GPS lap crossing -> lap {}", locked.lap_count);
                                    }
                                }
                                locked.last_gps = Some(cur);
                            }
                        }
                        Err(e) => eprintln!("[DASHD] Protobuf decode error: {}", e),
                    }
                }
            }
            Err(_) => {
                // cand not running yet; wait and retry
            }
        }

        thread::sleep(Duration::from_secs(2));
    }
}

// ---------------------------------------------------------------------------
// Thread 2: WebSocket server — sends DashMessage JSON to browser at ~30 Hz
// ---------------------------------------------------------------------------

fn ws_server_loop(state: Arc<Mutex<DashState>>) {
    let addr = format!("0.0.0.0:{}", WS_PORT);
    let listener = TcpListener::bind(&addr)
        .unwrap_or_else(|e| panic!("[DASHD] Failed to bind WebSocket on {}: {}", addr, e));
    println!("[DASHD] WebSocket server listening on {}", addr);

    let send_interval = Duration::from_millis(1000 / WS_SEND_HZ);
    let mut seq: u64 = 0;

    // Accept one client at a time. When a client disconnects, loop back to
    // accept the next one. seq persists across reconnections so the frontend
    // can detect gaps.
    for tcp_stream in listener.incoming() {
        match tcp_stream {
            Ok(tcp) => {
                println!("[DASHD] WebSocket client connected");
                match tungstenite::accept(tcp) {
                    Ok(mut ws) => {
                        loop {
                            let tick_start = Instant::now();
                            seq += 1;

                            let message = {
                                let locked = state.lock().unwrap();
                                let can = if locked.last_can_update.elapsed() > CAN_STALE_TIMEOUT {
                                    CanData::default()
                                } else {
                                    locked.can.clone()
                                };
                                let mut mqtt = locked.mqtt.to_mqtt_data();
                                // lap_trigger is the local monotonic lap count
                                // (GPS- or trackside-driven). Always fresh.
                                mqtt.lap_trigger = Some(locked.lap_count as f32);
                                let pacing = locked.pacing(Instant::now());
                                let layout = locked.layout.clone();
                                let park_layout = locked.park_layout.clone();
                                DashMessage { seq, can, mqtt, pacing, layout, park_layout }
                            };

                            let json = match serde_json::to_string(&message) {
                                Ok(j) => j,
                                Err(e) => {
                                    eprintln!("[DASHD] JSON serialize error: {}", e);
                                    continue;
                                }
                            };

                            // DEBUG (driveday): log just the power value that's
                            // about to be sent to chromium, every ~10 ticks
                            // (~3 Hz at 30Hz sender) so we catch transient
                            // accel/regen moments. Revert when kW=0 bug found.
                            {
                                use std::sync::atomic::{AtomicU32, Ordering};
                                static JSON_TICK: AtomicU32 = AtomicU32::new(0);
                                let n = JSON_TICK.fetch_add(1, Ordering::Relaxed);
                                if n % 10 == 0 {
                                    eprintln!("[DASHD-OUT] seq={} power={:?}", message.seq, message.can.power);
                                }
                            }

                            if ws.send(tungstenite::Message::Text(json)).is_err() {
                                println!("[DASHD] WebSocket client disconnected");
                                break;
                            }

                            let elapsed = tick_start.elapsed();
                            if elapsed < send_interval {
                                thread::sleep(send_interval - elapsed);
                            }
                        }
                    }
                    Err(e) => eprintln!("[DASHD] WebSocket handshake failed: {}", e),
                }
            }
            Err(e) => eprintln!("[DASHD] TCP accept error: {}", e),
        }
    }
}

// ---------------------------------------------------------------------------
// Thread 3: MQTT subscriber — receives off-car computed values
// ---------------------------------------------------------------------------

fn mqtt_subscriber_loop(state: Arc<Mutex<DashState>>) {
    let mqtt_host = env_or_default("DASHD_MQTT_HOST", MQTT_HOST);
    let mqtt_port: u16 = std::env::var("DASHD_MQTT_PORT")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(MQTT_PORT);

    loop {
        println!(
            "[DASHD] Connecting to MQTT broker {}:{}...",
            mqtt_host, mqtt_port
        );

        // Append PID so multiple dashd instances (e.g. dev laptop + on-car)
        // don't kick each other off the broker via duplicate client ids.
        let client_id = format!("{}-{}", MQTT_CLIENT_ID, std::process::id());
        let mut opts = MqttOptions::new(client_id, &mqtt_host, mqtt_port);
        opts.set_keep_alive(Duration::from_secs(20));

        let (client, mut connection) = Client::new(opts, 64);

        if let Err(e) = client.subscribe(
            format!("{}#", MQTT_TOPIC_PREFIX),
            QoS::AtMostOnce,
        ) {
            eprintln!("[DASHD] MQTT subscribe error: {}, will retry", e);
            thread::sleep(Duration::from_secs(5));
            continue;
        }

        println!("[DASHD] MQTT subscribed to {}#", MQTT_TOPIC_PREFIX);

        let mut connected = true;
        for event in connection.iter() {
            match event {
                Ok(Event::Incoming(Incoming::Publish(publish))) => {
                    let topic = &publish.topic;
                    let Some(field) = topic.strip_prefix(MQTT_TOPIC_PREFIX) else {
                        continue;
                    };

                    let payload_str = match std::str::from_utf8(&publish.payload) {
                        Ok(s) => s.trim(),
                        Err(_) => continue,
                    };

                    // sfGate carries a JSON array [lat1,lon1,lat2,lon2], not a
                    // bare number — handle it before the numeric parse. It's
                    // retained, so the dash re-loads the current gate on every
                    // reconnect; we also cache it to disk for reboot survival.
                    if field == "sfGate" {
                        match serde_json::from_str::<[f64; 4]>(payload_str) {
                            Ok(gate) => {
                                {
                                    let mut locked = state.lock().unwrap();
                                    locked.sf_gate = Some(gate);
                                    // Drop the last fix so swapping gates can't
                                    // manufacture a phantom crossing.
                                    locked.last_gps = None;
                                }
                                persist_sf_gate(&gate);
                                // Ack receipt (retained) so trackside can confirm
                                // the car actually got the gate.
                                let _ = client.publish(
                                    format!("{}ack/sfGate", MQTT_TOPIC_PREFIX),
                                    QoS::AtMostOnce,
                                    true,
                                    payload_str.as_bytes().to_vec(),
                                );
                                println!("[DASHD] Loaded start/finish gate: {:?}", gate);
                            }
                            Err(e) => {
                                eprintln!("[DASHD] MQTT bad sfGate payload {:?}: {}", payload_str, e)
                            }
                        }
                        continue;
                    }

                    // layout carries the lap-card layout as a JSON object; retained,
                    // so the dash re-loads it on reconnect. Cached to disk for reboot.
                    // We don't interpret it here — the frontend renders it.
                    if field == "layout" {
                        match serde_json::from_str::<serde_json::Value>(payload_str) {
                            Ok(v) => {
                                {
                                    let mut locked = state.lock().unwrap();
                                    locked.layout = Some(v);
                                }
                                persist_layout(payload_str);
                                let _ = client.publish(
                                    format!("{}ack/layout", MQTT_TOPIC_PREFIX),
                                    QoS::AtMostOnce,
                                    true,
                                    payload_str.as_bytes().to_vec(),
                                );
                                println!("[DASHD] Loaded lap-card layout ({} bytes)", payload_str.len());
                            }
                            Err(e) => {
                                eprintln!("[DASHD] MQTT bad layout payload: {}", e)
                            }
                        }
                        continue;
                    }

                    // parkLayout: the park/pit-screen layout. Same handling as
                    // `layout` — retained, cached to disk, forwarded verbatim.
                    if field == "parkLayout" {
                        match serde_json::from_str::<serde_json::Value>(payload_str) {
                            Ok(v) => {
                                {
                                    let mut locked = state.lock().unwrap();
                                    locked.park_layout = Some(v);
                                }
                                persist_park_layout(payload_str);
                                let _ = client.publish(
                                    format!("{}ack/parkLayout", MQTT_TOPIC_PREFIX),
                                    QoS::AtMostOnce,
                                    true,
                                    payload_str.as_bytes().to_vec(),
                                );
                                println!("[DASHD] Loaded park layout ({} bytes)", payload_str.len());
                            }
                            Err(e) => {
                                eprintln!("[DASHD] MQTT bad parkLayout payload: {}", e)
                            }
                        }
                        continue;
                    }

                    // We subscribe to lhre/dash/# and therefore hear our own
                    // publishes echoed back (state @2Hz + ack/* retained). Those
                    // aren't numeric inputs — skip them silently so they don't
                    // spam "bad payload" every frame.
                    if field == "state" || field.starts_with("ack/") {
                        continue;
                    }

                    let val: f32 = match payload_str.parse() {
                        Ok(v) => v,
                        Err(_) => {
                            eprintln!(
                                "[DASHD] MQTT bad payload on {}: {:?}",
                                topic, payload_str
                            );
                            continue;
                        }
                    };

                    let now = Instant::now();
                    let mut locked = state.lock().unwrap();
                    match field {
                        "lapDelta" => {
                            locked.mqtt.lap_delta = Some(val);
                            locked.mqtt.last_lap_delta = now;
                        }
                        "energyDelta" => {
                            locked.mqtt.energy_delta = Some(val);
                            locked.mqtt.last_energy_delta = now;
                        }
                        "lapsRemaining" => {
                            locked.mqtt.laps_remaining = Some(val);
                            locked.mqtt.last_laps_remaining = now;
                        }
                        "lapCardMs" => {
                            // How long the full-screen lap card stays up (ms).
                            // Retained config — held last-known, no staleness.
                            locked.mqtt.lap_card_ms = Some(val);
                        }
                        "targetPower" => {
                            locked.mqtt.target_power = Some(val);
                            locked.mqtt.last_target_power = now;
                            // Echo back so trackside knows the car heard the budget.
                            let _ = client.publish(
                                format!("{}ack/targetPower", MQTT_TOPIC_PREFIX),
                                QoS::AtMostOnce,
                                true,
                                val.to_string(),
                            );
                        }
                        "lapTrigger" => {
                            // Trackside is the AUTHORITATIVE lap count. The
                            // website publishes its `liveState.laps.length`
                            // (the absolute number of completed laps from its
                            // own counter); we adopt it directly into
                            // `lap_count` so the dash and the website always
                            // display the same number.
                            //
                            // Treatment:
                            //   - +1 increment (normal lap)  -> complete_lap,
                            //     fire the dash card. This MUST cover lap 1
                            //     too (the prior 'first publish = silent
                            //     baseline' branch swallowed every session's
                            //     first card). lapReset already drops
                            //     lap_count to 0, so 1 > 0 fires correctly.
                            //   - multi-lap jump  -> adopt silently. The
                            //     trackside drain re-publishes the website's
                            //     authoritative count after a dashd reboot,
                            //     and we don't want a card with bogus
                            //     time/energy for laps that never ran here.
                            //   - same or backwards -> ignore (republish of a
                            //     value we already processed).
                            let new_count = val.round().max(0.0) as u64;
                            locked.last_offcar_trigger = Some(val);
                            if new_count > locked.lap_count + 1 {
                                locked.lap_count = new_count;
                                println!("[DASHD] Trackside lapTrigger {} adopted silently (multi-lap jump from {})", val, locked.lap_count);
                            } else if new_count > locked.lap_count {
                                locked.complete_lap(now, Some(new_count));
                                let lap = locked.lap_count;
                                println!("[DASHD] Trackside lapTrigger {} -> lap {}", val, lap);
                                let _ = client.publish(
                                    format!("{}ack/lapTrigger", MQTT_TOPIC_PREFIX),
                                    QoS::AtMostOnce,
                                    true,
                                    lap.to_string(),
                                );
                            }
                        }
                        "lapReset" => {
                            // Trackside started a new session: drop both the
                            // baseline and the running lap_count. The next
                            // lapTrigger publish (typically 1) becomes the new
                            // baseline at lap 1, not "ignored because
                            // 1 < stale_count". Without this a fresh website
                            // session would have its first ~N clicks silently
                            // dropped while dashd was still pinned at the prior
                            // session's high water mark.
                            locked.last_offcar_trigger = None;
                            locked.lap_count = 0;
                            locked.lap_energy_wh = 0.0;
                            locked.lap_budget_wh = 0.0;
                            locked.lap_start = now;
                            locked.last_lap = None;
                            println!("[DASHD] Trackside lapReset -> counters cleared");
                        }
                        _ => {} // ignore unknown subtopics
                    }
                }
                Ok(Event::Incoming(Incoming::Disconnect)) => {
                    println!("[DASHD] MQTT broker disconnected, will reconnect");
                    connected = false;
                    break;
                }
                Ok(_) => {}
                Err(e) => {
                    eprintln!("[DASHD] MQTT connection error: {}", e);
                    connected = false;
                    break;
                }
            }
        }

        if !connected {
            thread::sleep(Duration::from_secs(5));
        }
    }
}

// ---------------------------------------------------------------------------
// Thread 4: MQTT state publisher — pushes the driver-facing snapshot up to
// `lhre/dash/state` so the trackside team can mirror exactly what the driver
// sees and confirm the uplink. Separate client from the subscriber so the
// blocking subscribe loop and the timed publish loop don't fight.
// ---------------------------------------------------------------------------

fn mqtt_state_publisher_loop(state: Arc<Mutex<DashState>>) {
    use std::sync::atomic::{AtomicBool, Ordering};

    let mqtt_host = env_or_default("DASHD_MQTT_HOST", MQTT_HOST);
    let mqtt_port: u16 = std::env::var("DASHD_MQTT_PORT")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(MQTT_PORT);

    let interval = Duration::from_millis(1000 / STATE_PUBLISH_HZ);

    loop {
        let client_id = format!("{}-PUB-{}", MQTT_CLIENT_ID, std::process::id());
        let mut opts = MqttOptions::new(client_id, &mqtt_host, mqtt_port);
        opts.set_keep_alive(Duration::from_secs(20));
        let (client, mut connection) = Client::new(opts, 16);

        // Drive the eventloop on a helper thread so outgoing publishes actually
        // flush; it flips `alive` to false the moment the link drops.
        let alive = Arc::new(AtomicBool::new(true));
        let alive_driver = Arc::clone(&alive);
        let driver = thread::spawn(move || {
            for event in connection.iter() {
                match event {
                    Ok(Event::Incoming(Incoming::Disconnect)) | Err(_) => break,
                    _ => {}
                }
            }
            alive_driver.store(false, Ordering::SeqCst);
        });

        while alive.load(Ordering::SeqCst) {
            let json = {
                let locked = state.lock().unwrap();
                let now = Instant::now();
                let msg = DashStateMsg {
                    lap_count: locked.lap_count,
                    target_power: locked.mqtt.target_power,
                    target_power_stale: locked
                        .mqtt
                        .target_power
                        .is_some_and(|_| {
                            now.duration_since(locked.mqtt.last_target_power) >= MQTT_STALE_TIMEOUT
                        }),
                    speed: locked.can.speed,
                    power: locked.can.power,
                    soc: locked.can.soc,
                    temperature: locked.can.temperature,
                    pacing: locked.pacing(now),
                    disk_free_mb: locked.disk_free_mb,
                    disk_total_mb: locked.disk_total_mb,
                    runtime_s: Some(locked.boot.elapsed().as_secs_f32()),
                };
                serde_json::to_string(&msg)
            };

            if let Ok(j) = json {
                if client
                    .publish(format!("{}state", MQTT_TOPIC_PREFIX), QoS::AtMostOnce, false, j)
                    .is_err()
                {
                    break;
                }
            }
            thread::sleep(interval);
        }

        let _ = client.disconnect();
        let _ = driver.join();
        thread::sleep(Duration::from_secs(5));
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Thread 5: disk monitor — polls free space on `/` so trackside can watch the
// storage kill switch (start_telemetry.sh stops telemetry when free < 1024 MB).
// ---------------------------------------------------------------------------

/// (free_mb, total_mb) on `/` via `df -k` — the same filesystem + metric the
/// kill switch reads (`df / | awk 'NR==2 {print $4/1024}'`). None on parse fail.
fn read_disk_mb() -> Option<(f64, f64)> {
    let out = std::process::Command::new("df").arg("-k").arg("/").output().ok()?;
    let text = String::from_utf8_lossy(&out.stdout);
    // Header line, then the root filesystem row:
    //   Filesystem  1K-blocks  Used  Available  Use%  Mounted-on
    let cols: Vec<&str> = text.lines().nth(1)?.split_whitespace().collect();
    let total_kb: f64 = cols.get(1)?.parse().ok()?;
    let avail_kb: f64 = cols.get(3)?.parse().ok()?;
    Some((avail_kb / 1024.0, total_kb / 1024.0))
}

fn disk_monitor_loop(state: Arc<Mutex<DashState>>) {
    loop {
        if let Some((free_mb, total_mb)) = read_disk_mb() {
            let mut locked = state.lock().unwrap();
            locked.disk_free_mb = Some(free_mb);
            locked.disk_total_mb = Some(total_mb);
        }
        // The kill switch checks every 60 s; 15 s here gives trackside a little
        // more lead time without being chatty.
        thread::sleep(Duration::from_secs(15));
    }
}

fn main() -> Result<()> {
    let state = Arc::new(Mutex::new(DashState {
        can: CanData::default(),
        boot: Instant::now(),
        disk_free_mb: None,
        disk_total_mb: None,
        // Start stale so the dash shows "--" until cand actually connects.
        last_can_update: Instant::now() - CAN_STALE_TIMEOUT,
        last_qualified_soc: None,
        mqtt: MqttState::new(),
        // Restore the last gate from disk; the retained MQTT topic will refresh
        // it once the broker connects.
        sf_gate: load_sf_gate(),
        last_gps: None,
        lap_count: 0,
        last_offcar_trigger: None,
        lap_energy_wh: 0.0,
        lap_budget_wh: 0.0,
        lap_start: Instant::now(),
        last_energy_update: None,
        last_lap: None,
        // Restore the last layout from disk; the retained MQTT topic refreshes it.
        layout: load_layout(),
        park_layout: load_park_layout(),
    }));

    let ipc_state = Arc::clone(&state);
    thread::spawn(move || ipc_reader_loop(ipc_state));

    let ws_state = Arc::clone(&state);
    thread::spawn(move || ws_server_loop(ws_state));

    let mqtt_state = Arc::clone(&state);
    thread::spawn(move || mqtt_subscriber_loop(mqtt_state));

    let pub_state = Arc::clone(&state);
    thread::spawn(move || mqtt_state_publisher_loop(pub_state));

    let disk_state = Arc::clone(&state);
    thread::spawn(move || disk_monitor_loop(disk_state));

    println!(
        "[DASHD] Started (IPC reader + WebSocket on :{} + MQTT subscriber + state publisher)",
        WS_PORT
    );

    // Park the main thread forever (matches cand/main.rs pattern)
    loop {
        thread::park();
    }
}
