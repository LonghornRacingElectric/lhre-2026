use anyhow::{Context, Result};
use prost::Message;
use serde::Deserialize;
use socketcan::{CanFrame, CanSocket, Socket};
use std::collections::HashMap;
use std::fs;
use std::io::Write;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

pub mod proto {
    include!(concat!(env!("OUT_DIR"), "/sensor_data.rs"));
}

use proto::sensor_data::{self, SensorData};

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const CAN_INTERFACE: &str = "vcan0";
const CAN_CONFIG_PATH: &str = "drivers/proto/can_packets.json";

mod config;
use config::{PacketConfig, SignalConfig, ProtobufMapping, BitfieldMapping};

fn main() -> Result<()> {
    // --- Load CAN Configuration ---
    let config_content = fs::read_to_string(CAN_CONFIG_PATH)
        .with_context(|| format!("Failed to read CAN config file at {}", CAN_CONFIG_PATH))?;
    let packets: Vec<PacketConfig> = serde_json::from_str(&config_content)?;
    let packet_map: HashMap<u32, PacketConfig> =
        packets.into_iter().map(|p| (p.packet_id, p)).collect();
    let arc_packet_map = Arc::new(packet_map);

    // --- Shared Data ---
    let sensor_data = Arc::new(Mutex::new(SensorData::default()));

    // --- CAN Thread ---
    let can_sensor_data = Arc::clone(&sensor_data);
    let can_thread = thread::spawn(move || {
        if let Err(e) = can_reader_loop(can_sensor_data, arc_packet_map) {
            eprintln!("[CAND-CAN] Error: {}", e);
        }
    });

    // --- IPC Server Thread ---
    let ipc_sensor_data = Arc::clone(&sensor_data);
    let ipc_thread = thread::spawn(move || {
        if let Err(e) = ipc_server_loop(ipc_sensor_data) {
            eprintln!("[CAND-IPC] Error: {}", e);
        }
    });

    println!("[CAND] Main thread waiting for child threads to complete.");
    can_thread.join().expect("CAN thread panicked");
    ipc_thread.join().expect("IPC thread panicked");

    Ok(())
}

    sensor_data: Arc<Mutex<SensorData>>,
    packet_map: Arc<HashMap<u32, PacketConfig>>,
) -> Result<()> {
    let socket = CanSocket::open(CAN_INTERFACE)
        .with_context(|| format!("Failed to open CAN socket on '{}'", CAN_INTERFACE))?;
    println!("[CAND-CAN] Listening for CAN frames on '{}'...", CAN_INTERFACE);

    loop {
        let frame = socket.read_frame().context("Failed to read CAN frame")?;
        if let Some(id) = frame.id_standard() {
            if let Some(config) = packet_map.get(&(id.as_raw() as u32)) {
                let mut data_guard = sensor_data.lock().expect("Mutex lock poisoned");
                process_frame(&mut data_guard, &frame, config);
            }
        }
    }
}

fn ipc_server_loop(sensor_data: Arc<Mutex<SensorData>>) -> Result<()> {
    if Path::new(SOCKET_PATH).exists() {
        fs::remove_file(SOCKET_PATH)?;
    }
    let listener = UnixListener::bind(SOCKET_PATH)?;
    println!("[CAND-IPC] Server listening at {}", SOCKET_PATH);

    let clients = Arc::new(Mutex::new(Vec::new()));

    // Client acceptor thread
    let clients_clone = Arc::clone(&clients);
    thread::spawn(move || accept_clients(listener, clients_clone));

    // Broadcast loop
    loop {
        let mut clients_guard = clients.lock().expect("Mutex lock poisoned");
        let data_guard = sensor_data.lock().expect("Mutex lock poisoned");
        
        let mut buffer = Vec::new();
        data_guard.encode(&mut buffer)?;

        clients_guard.retain_mut(|stream| {
            if stream.write_all(&buffer).is_err() {
                println!("[CAND-IPC] Client disconnected");
                false // Remove client
            } else {
                true
            }
        });

        drop(clients_guard);
        drop(data_guard);

        thread::sleep(Duration::from_millis(10)); 
    }
}

fn accept_clients(listener: UnixListener, clients: Arc<Mutex<Vec<UnixStream>>>) {
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                println!("[CAND-IPC] New client connected");
                clients.lock().expect("Mutex lock poisoned").push(stream);
            }
            Err(err) => eprintln!("[CAND-IPC] Connection error: {}", err),
        }
    }
}

fn process_frame(data: &mut SensorData, frame: &CanFrame, config: &PacketConfig) {
    let payload = frame.data();

    for signal in &config.bytes {
        if signal.start_byte + signal.length > payload.len() {
            continue;
        }

        match signal.conv_type.as_str() {
            "uint16" => {
                let val_bytes = &payload[signal.start_byte..signal.start_byte + 2];
                let raw = u16::from_le_bytes([val_bytes[0], val_bytes[1]]);
                let val = (raw as f64) * signal.precision;
                if let Some(pb_map) = &signal.protobuf {
                    update_proto_field(data, &pb_map.field_name, val as f32, pb_map);
                }
            }
            "int16" => {
                let val_bytes = &payload[signal.start_byte..signal.start_byte + 2];
                let raw = i16::from_le_bytes([val_bytes[0], val_bytes[1]]);
                let val = (raw as f64) * signal.precision;
                if let Some(pb_map) = &signal.protobuf {
                    update_proto_field(data, &pb_map.field_name, val as f32, pb_map);
                }
            }
            "uint8" => {
                let raw = payload[signal.start_byte];
                let val = (raw as f64) * signal.precision;
                if let Some(pb_map) = &signal.protobuf {
                    update_proto_field(data, &pb_map.field_name, val as f32, pb_map);
                }
            }
            "bitfield" => {
                let raw = payload[signal.start_byte];
                if let Some(mappings) = &signal.bitfield_encoding {
                    for map in mappings {
                        let is_set = ((raw >> map.bit_index) & 1) != 0;
                        update_proto_bool(data, &map.protobuf_field, is_set);
                    }
                }
            }
            _ => eprintln!("[CAND-CAN] Unknown signal conversion type: {}", signal.conv_type),
        }
    }
}

/// Updates a float field in the SensorData protobuf message.
fn update_proto_field(data: &mut SensorData, field_name: &str, value: f32, config: &ProtobufMapping) {
    let dyn_ref = data.dynamics.get_or_insert_with(Default::default);
    let ctr_ref = data.controls.get_or_insert_with(Default::default);
    let pack_ref = data.pack.get_or_insert_with(Default::default);

    if config.repeated {
        if let Some(idx) = config.field_index {
            match field_name {
                // --- Dynamics (Repeated) ---
                "fl_sprung_accel" => set_vec_index(&mut dyn_ref.fl_sprung_accel, idx, value),
                "fr_sprung_accel" => set_vec_index(&mut dyn_ref.fr_sprung_accel, idx, value),
                "fl_unsprung_accel" => set_vec_index(&mut dyn_ref.fl_unsprung_accel, idx, value),
                // Add other repeated fields here
                _ => (),
            }
        }
    } else {
        match field_name {
            // --- Dynamics ---
            "fl_steer_angle" => dyn_ref.fl_steer_angle = value,
            "fr_steer_angle" => dyn_ref.fr_steer_angle = value,
            "flw_speed" => dyn_ref.flw_speed = value,
            "frw_speed" => dyn_ref.frw_speed = value,
            
            // --- Controls ---
            "brake_pressure_f" => ctr_ref.brake_pressure_f = value,
            "brake_pressure_rall" => ctr_ref.brake_pressure_rall = value,
            
            // --- Pack ---
            "hv_pack_v" => pack_ref.hv_pack_v = value,
            "hv_c" => pack_ref.hv_c = value,
            "hv_soc" => pack_ref.hv_soc = value,
            
            _ => println!("[CAND-CAN] Warning: Unmapped field '{}'", field_name),
        }
    }
}

/// Updates a boolean field in the SensorData protobuf message.
fn update_proto_bool(data: &mut SensorData, field_name: &str, value: bool) {
    let diag_high = data.diagnostics_high.get_or_insert_with(Default::default);
    let diag_low = data.diagnostics_low.get_or_insert_with(Default::default);

    match field_name {
        "apps1_disconnect" => diag_high.apps1_disconnect = value,
        "apps2_disconnect" => diag_high.apps2_disconnect = value,
        "apps1_out_range" => diag_high.apps1_out_range = value,
        
        "bmb_comm_error" => diag_low.bmb_comm_error = value,
        "imd_gnd_isolation_error" => diag_low.imd_gnd_isolation_error = value,
        
        _ => (),
    }
}

fn set_vec_index(vec: &mut Vec<f32>, index: usize, value: f32) {
    if vec.len() <= index {
        vec.resize(index + 1, 0.0);
    }
    vec[index] = value;
}