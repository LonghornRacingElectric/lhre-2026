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

    /// Per-wheel speed in same units as `wheel_speed` (dynamics.flw/frw/blw/brw_speed).
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
}

#[derive(Serialize, Clone)]
struct DashMessage {
    seq: u64,
    can: CanData,
    mqtt: MqttData,
}

// ---------------------------------------------------------------------------
// Shared state between all threads
// ---------------------------------------------------------------------------

struct MqttState {
    lap_delta: Option<f32>,
    energy_delta: Option<f32>,
    laps_remaining: Option<f32>,
    last_lap_delta: Instant,
    last_energy_delta: Instant,
    last_laps_remaining: Instant,
}

impl MqttState {
    fn new() -> Self {
        let epoch = Instant::now() - MQTT_STALE_TIMEOUT; // start stale
        Self {
            lap_delta: None,
            energy_delta: None,
            laps_remaining: None,
            last_lap_delta: epoch,
            last_energy_delta: epoch,
            last_laps_remaining: epoch,
        }
    }

    /// Convert to the JSON-serializable MqttData, nulling out stale fields.
    fn to_mqtt_data(&self) -> MqttData {
        let now = Instant::now();
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

        wheel_speed_fl: dynamics.map(|d| d.flw_speed),
        wheel_speed_fr: dynamics.map(|d| d.frw_speed),
        wheel_speed_rl: dynamics.map(|d| d.blw_speed),
        wheel_speed_rr: dynamics.map(|d| d.brw_speed),

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
                            let mut locked = state.lock().unwrap();
                            let can_data = extract_can_data(&data, &mut locked.last_qualified_soc);
                            locked.can = can_data;
                            locked.last_can_update = Instant::now();
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
                                DashMessage {
                                    seq,
                                    can,
                                    mqtt: locked.mqtt.to_mqtt_data(),
                                }
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
// Main
// ---------------------------------------------------------------------------

fn main() -> Result<()> {
    let state = Arc::new(Mutex::new(DashState {
        can: CanData::default(),
        // Start stale so the dash shows "--" until cand actually connects.
        last_can_update: Instant::now() - CAN_STALE_TIMEOUT,
        last_qualified_soc: None,
        mqtt: MqttState::new(),
    }));

    let ipc_state = Arc::clone(&state);
    thread::spawn(move || ipc_reader_loop(ipc_state));

    let ws_state = Arc::clone(&state);
    thread::spawn(move || ws_server_loop(ws_state));

    let mqtt_state = Arc::clone(&state);
    thread::spawn(move || mqtt_subscriber_loop(mqtt_state));

    println!("[DASHD] Started (IPC reader + WebSocket on :{} + MQTT subscriber)", WS_PORT);

    // Park the main thread forever (matches cand/main.rs pattern)
    loop {
        thread::park();
    }
}
