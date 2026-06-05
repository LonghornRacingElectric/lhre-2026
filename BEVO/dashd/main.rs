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
}

// ---------------------------------------------------------------------------
// Shared state between all threads
// ---------------------------------------------------------------------------

struct MqttState {
    lap_delta: Option<f32>,
    energy_delta: Option<f32>,
    laps_remaining: Option<f32>,
    target_power: Option<f32>,
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
        }
    }
}

struct DashState {
    can: CanData,
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
}

impl DashState {
    /// Close the in-progress lap: snapshot its time + energy for the card,
    /// bump the counter, and reset the per-lap integrators. Called from both
    /// lap sources (on-car GPS crossing, trackside lapTrigger) so the energy
    /// reset can never drift from the lap boundary.
    fn complete_lap(&mut self, now: Instant) {
        let completed = (self.lap_count + 1) as u32;
        self.last_lap = Some((
            completed,
            now.duration_since(self.lap_start).as_secs_f32(),
            self.lap_energy_wh as f32,
        ));
        self.lap_count += 1;
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

    // Per-cell temp aggregates — None when cand has not received cell-temp
    // packets yet (empty vec).
    let (cell_temp_max, cell_temp_avg, cell_temp_min) = pack
        .map(|p| p.cells_temps.as_slice())
        .filter(|t| !t.is_empty())
        .map(|t| {
            let max = t.iter().cloned().fold(f32::NEG_INFINITY, f32::max);
            let min = t.iter().cloned().fold(f32::INFINITY, f32::min);
            let avg = t.iter().sum::<f32>() / t.len() as f32;
            (Some(max), Some(avg), Some(min))
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
                                        locked.complete_lap(now);
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
                                DashMessage { seq, can, mqtt, pacing }
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
                            // Trackside override / fallback for the on-car GPS
                            // detector: each rising edge closes a lap. First
                            // value just sets the baseline (no phantom lap on a
                            // retained/stale value already on the broker).
                            let rising = locked.last_offcar_trigger.is_some_and(|last| val > last);
                            locked.last_offcar_trigger = Some(val);
                            if rising {
                                locked.complete_lap(now);
                                let lap = locked.lap_count;
                                println!("[DASHD] Trackside lapTrigger -> lap {}", lap);
                                let _ = client.publish(
                                    format!("{}ack/lapTrigger", MQTT_TOPIC_PREFIX),
                                    QoS::AtMostOnce,
                                    true,
                                    lap.to_string(),
                                );
                            }
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

fn main() -> Result<()> {
    let state = Arc::new(Mutex::new(DashState {
        can: CanData::default(),
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
    }));

    let ipc_state = Arc::clone(&state);
    thread::spawn(move || ipc_reader_loop(ipc_state));

    let ws_state = Arc::clone(&state);
    thread::spawn(move || ws_server_loop(ws_state));

    let mqtt_state = Arc::clone(&state);
    thread::spawn(move || mqtt_subscriber_loop(mqtt_state));

    let pub_state = Arc::clone(&state);
    thread::spawn(move || mqtt_state_publisher_loop(pub_state));

    println!(
        "[DASHD] Started (IPC reader + WebSocket on :{} + MQTT subscriber + state publisher)",
        WS_PORT
    );

    // Park the main thread forever (matches cand/main.rs pattern)
    loop {
        thread::park();
    }
}
