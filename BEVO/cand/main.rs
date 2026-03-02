use anyhow::Result;
use prost::Message;
use socketcan::{CanSocket, Socket, EmbeddedFrame, Id};
use std::collections::HashMap;
use std::io::Write;
use std::net::UdpSocket;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::Path;
use std::fs;
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use sensor_proto::proto::orion::OrionSensorData;
use sensor_proto::config::PacketConfig;
use sensor_proto::generated_mapping;

const MOCK_ADDR: &str = "127.0.0.1:5005";
const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const STARTUP_SEMAPHORE_PATH: &str = "/tmp/BEVO_publishd_ready";
const CAN_INTERFACE: &str = "can0";
const DEFAULT_PUBLISH_HZ: u64 = 100;

const DEFAULT_CAN_JSON_PATH: &str = "BEVO/nonhermetic/assets/can.json";

#[derive(Debug)]
struct RawCanMessage {
    id: u32,
    payload: Vec<u8>,
}

fn main() -> Result<()> {
    let use_mock = matches!(
        std::env::var("CAND_USE_MOCK")
            .ok()
            .as_deref()
            .map(|value| value.to_ascii_lowercase()),
        Some(value) if value == "1" || value == "true" || value == "yes"
    );
    let can_interface = std::env::var("CAND_CAN_INTERFACE").unwrap_or_else(|_| CAN_INTERFACE.to_string());
    let publish_hz = std::env::var("CAND_PUBLISH_HZ")
        .ok()
        .and_then(|value| value.parse::<u64>().ok())
        .filter(|value| *value > 0)
        .unwrap_or(DEFAULT_PUBLISH_HZ);

    let config_json = load_can_config_json()?;
    let packets: Vec<PacketConfig> = serde_json::from_str(&config_json)?;
    let initial_packet_id = if use_mock {
        1
    } else {
        wait_for_publishd_ready()?
    };
    
    // This is where CAN IDs are mapped to their JSON configuration
    let packet_map = Arc::new(
        packets.into_iter()
            .map(|p| (p.packet_id, p))
            .collect::<HashMap<u32, PacketConfig>>()
    );

    let sensor_data_cache = Arc::new(Mutex::new(OrionSensorData::default()));
    let (raw_tx, raw_rx) = mpsc::channel::<RawCanMessage>();

    let can_packet_map = Arc::clone(&packet_map);
    let can_interface_clone = can_interface.clone();
    let can_reader_tx = raw_tx.clone();
    thread::spawn(move || {
        if let Err(e) = can_reader_loop(can_reader_tx, use_mock, can_interface_clone) {
            eprintln!("[CAND-CAN] Error: {:?}", e);
        }
    });

    let processing_sensor_data = Arc::clone(&sensor_data_cache);
    thread::spawn(move || {
        can_processing_loop(processing_sensor_data, can_packet_map, raw_rx);
    });

    let ipc_sensor_data = Arc::clone(&sensor_data_cache);
    thread::spawn(move || {
        if let Err(e) = ipc_server_loop(ipc_sensor_data, publish_hz, initial_packet_id) {
            eprintln!("[CAND-IPC] Error: {:?}", e);
        }
    });

    if use_mock {
        println!("[CAND] Started in MOCK mode ({}) @ {} Hz", MOCK_ADDR, publish_hz);
    } else {
        println!("[CAND] Started in REAL mode ({}) @ {} Hz", can_interface, publish_hz);
    }
    loop { thread::park(); }
}

fn load_can_config_json() -> Result<String> {
    let configured_path = std::env::var("CAND_CAN_JSON_PATH").unwrap_or_else(|_| DEFAULT_CAN_JSON_PATH.to_string());
    let config_json = fs::read_to_string(&configured_path)?;
    Ok(config_json)
}

fn wait_for_publishd_ready() -> Result<u64> {
    println!("[CAND] Waiting for publishd startup semaphore at {}", STARTUP_SEMAPHORE_PATH);
    loop {
        if Path::new(STARTUP_SEMAPHORE_PATH).exists() {
            if let Ok(contents) = std::fs::read_to_string(STARTUP_SEMAPHORE_PATH) {
                if let Ok(packet_id) = contents.trim().parse::<u64>() {
                    println!("[CAND] publishd ready, starting from packet_id={}", packet_id);
                    return Ok(packet_id.max(1));
                }
            }
        }
        thread::sleep(Duration::from_millis(100));
    }
}

// continuously reads CAN frames from the specified interface (or mock UDP socket), and sends them to the processing thread over a channel

fn can_reader_loop(raw_tx: Sender<RawCanMessage>, use_mock: bool, can_interface: String) -> Result<()> {
    if use_mock {
        let socket = UdpSocket::bind(MOCK_ADDR)?;
        let mut buf = [0u8; 12];
        loop {
            socket.recv_from(&mut buf)?;
            let id = u32::from_le_bytes(buf[0..4].try_into().unwrap());
            let payload = buf[4..12].to_vec();
            if raw_tx.send(RawCanMessage { id, payload }).is_err() {
                return Ok(());
            }
        }
    } else {
        let socket = CanSocket::open(&can_interface)?;
        loop {
            let frame = socket.read_frame()?;
            if let Id::Standard(id) = frame.id() {
                let payload = frame.data().to_vec();
                let message = RawCanMessage {
                    id: id.as_raw() as u32,
                    payload,
                };
                if raw_tx.send(message).is_err() {
                    return Ok(());
                }
            }
        }
    }
}

// takes raw CAN messages from the reader thread, looks up their config, and updates the proto struct in-place with the new values

fn can_processing_loop(
    data_cache: Arc<Mutex<OrionSensorData>>,
    packet_map: Arc<HashMap<u32, PacketConfig>>,
    raw_rx: Receiver<RawCanMessage>,
) {
    while let Ok(message) = raw_rx.recv() {
        if let Some(config) = packet_map.get(&message.id) {
            let mut locked = data_cache.lock().unwrap();
            process_raw_data(&mut locked, &message.payload, config);
        }
    }
}

// takes raw CAN data + the JSON config for that CAN ID, updates the proto struct in-place with the new values

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

/// send them jawns to dashd and publishd over a unix socket, at the specified publish_hz rate

fn ipc_server_loop(sensor_data: Arc<Mutex<OrionSensorData>>, publish_hz: u64, initial_packet_id: u64) -> Result<()> {
    let _ = std::fs::remove_file(SOCKET_PATH); 
    let listener = UnixListener::bind(SOCKET_PATH)?;
    let clients = Arc::new(Mutex::new(Vec::<UnixStream>::new()));
    let publish_interval = Duration::from_secs_f64(1.0 / publish_hz as f64);
    let mut next_packet_id = initial_packet_id.max(1);

    let clients_clone = Arc::clone(&clients);
    thread::spawn(move || {
        for stream in listener.incoming() {
            if let Ok(s) = stream { clients_clone.lock().unwrap().push(s); }
        }
    });

    loop {
        // add packet id and time fields 
        {
            let mut data = sensor_data.lock().unwrap();
            data.packet_id = (next_packet_id.min(i64::MAX as u64)) as i64;
            data.time = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_millis() as i64;
        }

        let cycle_start = Instant::now();
        let mut buffer = Vec::new();
        {
            sensor_data.lock().unwrap().encode(&mut buffer).ok();
        }

        let frame_len = (buffer.len() as u32).to_be_bytes();
        clients.lock().unwrap().retain_mut(|stream| {
            stream.write_all(&frame_len).and_then(|_| stream.write_all(&buffer)).is_ok()
        });
        next_packet_id = next_packet_id.saturating_add(1);

        let elapsed = cycle_start.elapsed();
        if elapsed < publish_interval {
            thread::sleep(publish_interval - elapsed);
        }
    }
}