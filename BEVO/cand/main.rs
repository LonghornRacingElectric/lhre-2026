use anyhow::Result;
use prost::Message;
use socketcan::{CanSocket, Socket, EmbeddedFrame, Id};
use std::collections::HashMap;
use std::io::Write;
use std::net::UdpSocket;
use std::os::unix::net::{UnixListener, UnixStream};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use sensor_proto::proto::OrionSensorData;
use sensor_proto::config::PacketConfig;
use sensor_proto::generated_mapping;

const MOCK_ADDR: &str = "127.0.0.1:5005";
const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const CAN_INTERFACE: &str = "can0";

const CONFIG_JSON: &str = include_str!(env!("CAND_CAN_JSON_PATH"));

fn main() -> Result<()> {
    let use_mock = matches!(
        std::env::var("CAND_USE_MOCK")
            .ok()
            .as_deref()
            .map(|value| value.to_ascii_lowercase()),
        Some(value) if value == "1" || value == "true" || value == "yes"
    );
    let can_interface = std::env::var("CAND_CAN_INTERFACE").unwrap_or_else(|_| CAN_INTERFACE.to_string());
    let packets: Vec<PacketConfig> = serde_json::from_str(CONFIG_JSON)?;
    
    // This is where CAN IDs are mapped to their JSON configuration
    let packet_map = Arc::new(
        packets.into_iter()
            .map(|p| (p.packet_id, p))
            .collect::<HashMap<u32, PacketConfig>>()
    );

    let sensor_data = Arc::new(Mutex::new(OrionSensorData::default()));

    let can_sensor_data = Arc::clone(&sensor_data);
    let can_packet_map = Arc::clone(&packet_map);
    let can_interface_clone = can_interface.clone();
    thread::spawn(move || {
        if let Err(e) = can_reader_loop(can_sensor_data, can_packet_map, use_mock, can_interface_clone) {
            eprintln!("[CAND-CAN] Error: {:?}", e);
        }
    });

    let ipc_sensor_data = Arc::clone(&sensor_data);
    thread::spawn(move || {
        if let Err(e) = ipc_server_loop(ipc_sensor_data) {
            eprintln!("[CAND-IPC] Error: {:?}", e);
        }
    });

    if use_mock {
        println!("[CAND] Started in MOCK mode ({})", MOCK_ADDR);
    } else {
        println!("[CAND] Started in REAL mode ({})", can_interface);
    }
    loop { thread::park(); }
}

fn can_reader_loop(data: Arc<Mutex<OrionSensorData>>, map: Arc<HashMap<u32, PacketConfig>>, use_mock: bool, can_interface: String) -> Result<()> {
    if use_mock {
        let socket = UdpSocket::bind(MOCK_ADDR)?;
        let mut buf = [0u8; 12];
        loop {
            socket.recv_from(&mut buf)?;
            let id = u32::from_le_bytes(buf[0..4].try_into().unwrap());
            if let Some(config) = map.get(&id) {
                let mut locked = data.lock().unwrap();
                process_raw_data(&mut locked, &buf[4..12], config);
            }
        }
    } else {
        let socket = CanSocket::open(&can_interface)?;
        loop {
            let frame = socket.read_frame()?;
            if let Id::Standard(id) = frame.id() {
                if let Some(config) = map.get(&(id.as_raw() as u32)) {
                    let mut locked = data.lock().unwrap();
                    process_raw_data(&mut locked, frame.data(), config);
                }
            }
        }
    }
}

fn process_raw_data(data: &mut OrionSensorData, payload: &[u8], config: &PacketConfig) {
    for signal in &config.bytes {
        if signal.start_byte + signal.length > payload.len() { continue; }
        
        if signal.conv_type == "bitfield" {
            if let Some(mappings) = &signal.bitfield_encoding {
                for m in mappings {
                    let is_set = ((payload[signal.start_byte] >> m.bit_index) & 1) != 0;
                    generated_mapping::update_proto_bool_generated(data, &m.protobuf_field, is_set);
                }
            }
            continue;
        }

        let val = match signal.conv_type.as_str() {
            "uint16" => {
                let bytes = [payload[signal.start_byte], payload[signal.start_byte+1]];
                (u16::from_le_bytes(bytes) as f64) * signal.precision
            },
            "int16" => {
                let bytes = [payload[signal.start_byte], payload[signal.start_byte+1]];
                (i16::from_le_bytes(bytes) as f64) * signal.precision
            },
            "uint8" => (payload[signal.start_byte] as f64) * signal.precision,
            "int8" => (payload[signal.start_byte] as i8 as f64) * signal.precision,
            _ => continue,
        };

        if let Some(pb) = &signal.protobuf { 
            generated_mapping::update_proto_field_generated(data, &pb.field_name, val as f32, pb); 
        }
    }
}

fn ipc_server_loop(sensor_data: Arc<Mutex<OrionSensorData>>) -> Result<()> {
    let _ = std::fs::remove_file(SOCKET_PATH); 
    let listener = UnixListener::bind(SOCKET_PATH)?;
    let clients = Arc::new(Mutex::new(Vec::<UnixStream>::new()));

    let clients_clone = Arc::clone(&clients);
    thread::spawn(move || {
        for stream in listener.incoming() {
            if let Ok(s) = stream { clients_clone.lock().unwrap().push(s); }
        }
    });

    loop {
        let mut buffer = Vec::new();
        { sensor_data.lock().unwrap().encode(&mut buffer).ok(); }
        clients.lock().unwrap().retain_mut(|s| s.write_all(&buffer).is_ok());
        thread::sleep(Duration::from_millis(10));
    }
}
pub fn set_vec_index(v: &mut Vec<f32>, i: usize, val: f32) {
    if v.len() <= i { v.resize(i + 1, 0.0); }
    v[i] = val;
}