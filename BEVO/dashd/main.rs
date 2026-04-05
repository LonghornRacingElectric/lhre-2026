use anyhow::Result;
use prost::Message;
use serde::Serialize;
use std::io::Read;
use std::net::TcpListener;
use std::os::unix::net::UnixStream;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use sensor_proto::proto::orion::OrionSensorData;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const WS_PORT: u16 = 8001;
const WS_SEND_HZ: u64 = 30;

// ---------------------------------------------------------------------------
// JSON schema structs — must match DashData.ts exactly
// ---------------------------------------------------------------------------

#[derive(Serialize, Clone, Default)]
struct CanData {
    /// Vehicle speed from dynamics.wheel_speed.
    /// UNIT UNKNOWN — raw CAN value passed through as-is (int16 * 0.0078125).
    /// Needs verification from electrical/controls team before display is meaningful.
    speed: Option<f32>,

    /// Electrical power in kW, derived as dc_bus_v * dc_bus_current / 1000.
    power: Option<f32>,

    /// NOT AVAILABLE — no CAN field for odometer.
    /// Would require integrating wheel_speed over time, and speed unit must be
    /// confirmed first. Always null.
    odometer: Option<f32>,

    /// State of charge (0–100%) from pack.hv_soc.
    soc: Option<f32>,

    /// Battery cell temperature in °C from thermal.cell_top_temp.
    /// DECISION: Using cell_top_temp. Alternatives: cell_bottom_temp,
    /// batt_loop_batt_temp, motor_temp. May need revisiting.
    temperature: Option<f32>,

    /// NOT AVAILABLE — 5G signal strength is not on the CAN bus. Always null.
    #[serde(rename = "signalStrength")]
    signal_strength: Option<f32>,

    /// NOT AVAILABLE — CAN provides 4 shutdown legs (shutdown_leg1–4 in
    /// DiagnosticsLow), but the frontend expects a 16-element boolean array
    /// matching specific named items. The mapping from 4 hardware legs to 16
    /// named shutdown items is UNKNOWN. Always null until electrical team
    /// provides this mapping.
    shutdown: Option<Vec<bool>>,
}

#[derive(Serialize, Clone, Default)]
struct MqttData {
    /// NOT AVAILABLE — comes from external MQTT timing system. Always null.
    #[serde(rename = "lapDelta")]
    lap_delta: Option<f32>,

    /// NOT AVAILABLE — comes from external MQTT strategy system. Always null.
    #[serde(rename = "energyDelta")]
    energy_delta: Option<f32>,

    /// NOT AVAILABLE — comes from external MQTT strategy system. Always null.
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
// Shared state between IPC reader and WebSocket sender
// ---------------------------------------------------------------------------

struct DashState {
    can: CanData,
    mqtt: MqttData,
}

// ---------------------------------------------------------------------------
// Protobuf -> CanData extraction
// ---------------------------------------------------------------------------

fn extract_can_data(data: &OrionSensorData) -> CanData {
    let speed = data.dynamics.as_ref().map(|d| d.wheel_speed);

    let power = data.pack.as_ref().map(|p| p.dc_bus_v * p.dc_bus_current / 1000.0);

    let soc = data.pack.as_ref().map(|p| p.hv_soc);

    let temperature = data.thermal.as_ref().map(|t| t.cell_top_temp);

    CanData {
        speed,
        power,
        soc,
        temperature,
        odometer: None,
        signal_strength: None,
        shutdown: None,
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
                            let can_data = extract_can_data(&data);
                            let mut locked = state.lock().unwrap();
                            locked.can = can_data;
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
                                DashMessage {
                                    seq,
                                    can: locked.can.clone(),
                                    mqtt: locked.mqtt.clone(),
                                }
                            };

                            let json = match serde_json::to_string(&message) {
                                Ok(j) => j,
                                Err(e) => {
                                    eprintln!("[DASHD] JSON serialize error: {}", e);
                                    continue;
                                }
                            };

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
// Main
// ---------------------------------------------------------------------------

fn main() -> Result<()> {
    let state = Arc::new(Mutex::new(DashState {
        can: CanData::default(),
        mqtt: MqttData::default(),
    }));

    let ipc_state = Arc::clone(&state);
    thread::spawn(move || ipc_reader_loop(ipc_state));

    let ws_state = Arc::clone(&state);
    thread::spawn(move || ws_server_loop(ws_state));

    println!("[DASHD] Started (IPC reader + WebSocket on :{})", WS_PORT);

    // Park the main thread forever (matches cand/main.rs pattern)
    loop {
        thread::park();
    }
}
