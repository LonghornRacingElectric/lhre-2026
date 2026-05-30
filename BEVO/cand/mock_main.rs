use anyhow::Result;
use prost::Message;
use sensor_proto::config::PacketConfig;
use sensor_proto::generated_mapping;
use sensor_proto::proto::orion::OrionSensorData;
use std::collections::HashMap;
use std::fs;
use std::io::Write;
use std::net::UdpSocket;
use std::os::unix::net::{UnixListener, UnixStream};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

const MOCK_PORT_0: &str = "127.0.0.1:5005";
const MOCK_PORT_1: &str = "127.0.0.1:5006";
const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const PUBLISHD_SOCKET_PATH: &str = "/tmp/BEVO_cand_publishd.sock";
const DEFAULT_CAN_JSON_PATH: &str = "BEVO/nonhermetic/assets/can.json";
const DEFAULT_PUBLISH_HZ: u64 = 100;

#[derive(Debug)]
struct RawCanMessage {
    id: u32,
    payload: Vec<u8>,
}

#[derive(Debug, Copy, Clone)]
struct PublishTarget {
    name: &'static str,
    socket_path: &'static str,
}

static DEFAULT_PUBLISH_TARGETS: &[PublishTarget] = &[
    PublishTarget {
        name: "cand",
        socket_path: SOCKET_PATH,
    },
    PublishTarget {
        name: "publishd",
        socket_path: PUBLISHD_SOCKET_PATH,
    },
];

fn selected_publish_targets() -> Vec<PublishTarget> {
    let configured = std::env::var("CAND_MOCK_PUBLISH_TARGETS").ok();
    let selected = configured
        .as_deref()
        .map(|value| {
            value
                .split(',')
                .map(|item| item.trim())
                .filter(|item| !item.is_empty())
                .collect::<Vec<_>>()
        })
        .filter(|items| !items.is_empty());

    match selected {
        Some(names) => DEFAULT_PUBLISH_TARGETS
            .iter()
            .copied()
            .filter(|target| names.iter().any(|name| *name == target.name))
            .collect(),
        None => DEFAULT_PUBLISH_TARGETS.to_vec(),
    }
}

fn load_can_config_json() -> Result<String> {
    let configured_path = std::env::var("CAND_CAN_JSON_PATH").unwrap_or_else(|_| DEFAULT_CAN_JSON_PATH.to_string());
    let config_json = fs::read_to_string(&configured_path)?;
    Ok(config_json)
}

fn repeated_field_span(config: &PacketConfig) -> usize {
    config
        .bytes
        .iter()
        .filter_map(|signal| {
            signal
                .protobuf
                .as_ref()
                .filter(|protobuf| protobuf.repeated)
                .and_then(|protobuf| protobuf.field_index)
        })
        .max()
        .map(|index| index + 1)
        .unwrap_or(0)
}

fn resolve_packet_config<'a>(
    packet_map: &'a HashMap<u32, PacketConfig>,
    message_id: u32,
) -> Option<(&'a PacketConfig, usize)> {
    if let Some(config) = packet_map.get(&message_id) {
        return Some((config, 0));
    }

    packet_map.values().find_map(|config| {
        let quantity = config.quantity.max(1);
        let end_id = config.packet_id.saturating_add(quantity);
        if message_id >= config.packet_id && message_id < end_id {
            Some((config, (message_id - config.packet_id) as usize))
        } else {
            None
        }
    })
}

fn process_raw_data(
    data: &mut OrionSensorData,
    payload: &[u8],
    config: &PacketConfig,
    series_index: usize,
) {
    let repeated_span = repeated_field_span(config);
    for signal in &config.bytes {
        if signal.start_byte + signal.length > payload.len() {
            continue;
        }

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
                let bytes = [payload[signal.start_byte], payload[signal.start_byte + 1]];
                (u16::from_le_bytes(bytes) as f64) * signal.precision
            }
            "int16" => {
                let bytes = [payload[signal.start_byte], payload[signal.start_byte + 1]];
                (i16::from_le_bytes(bytes) as f64) * signal.precision
            }
            "uint8" => (payload[signal.start_byte] as f64) * signal.precision,
            "int8" => (payload[signal.start_byte] as i8 as f64) * signal.precision,
            _ => continue,
        };

        if let Some(pb) = &signal.protobuf {
            if pb.repeated {
                if let Some(field_index) = pb.field_index {
                    let mut adjusted_pb = pb.clone();
                    adjusted_pb.field_index = Some(field_index + series_index.saturating_mul(repeated_span));
                    generated_mapping::update_proto_field_generated(
                        data,
                        &adjusted_pb.field_name,
                        val as f32,
                        &adjusted_pb,
                    );
                }
            } else {
                generated_mapping::update_proto_field_generated(data, &pb.field_name, val as f32, pb);
            }
        }
    }
}

fn can_reader_loop(raw_tx: Sender<RawCanMessage>, port: &'static str) -> Result<()> {
    let socket = UdpSocket::bind(port)?;
    let mut buf = [0u8; 12];
    loop {
        let (len, _) = socket.recv_from(&mut buf)?;
        if len < 4 {
            continue;
        }

        let id = u32::from_le_bytes(buf[0..4].try_into().unwrap());
        let payload = buf[4..12].to_vec();
        if raw_tx.send(RawCanMessage { id, payload }).is_err() {
            return Ok(());
        }
    }
}

fn spawn_ipc_server(socket_path: &str) -> Result<Arc<Mutex<Vec<UnixStream>>>> {
    let _ = fs::remove_file(socket_path);
    let listener = UnixListener::bind(socket_path)?;
    let clients = Arc::new(Mutex::new(Vec::<UnixStream>::new()));
    let clients_clone = Arc::clone(&clients);

    thread::spawn(move || {
        for stream in listener.incoming() {
            if let Ok(stream) = stream {
                clients_clone.lock().unwrap().push(stream);
            }
        }
    });

    Ok(clients)
}

fn ipc_publish_loop(
    sensor_data: Arc<Mutex<OrionSensorData>>,
    publish_targets: Vec<PublishTarget>,
    publish_hz: u64,
) -> Result<()> {
    let publish_interval = Duration::from_secs_f64(1.0 / publish_hz as f64);
    let mut next_packet_id: u64 = 1;

    let mut target_clients = Vec::new();
    for target in publish_targets {
        let clients = spawn_ipc_server(target.socket_path)?;
        println!("[mock_main] publishing on {} ({})", target.socket_path, target.name);
        target_clients.push((target, clients));
    }

    loop {
        let cycle_start = Instant::now();
        let mut buffer = Vec::new();
        {
            let mut data = sensor_data.lock().unwrap();
            data.time = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis() as i64;
            data.packet_id = (next_packet_id.min(i64::MAX as u64)) as i64;
            data.encode(&mut buffer).ok();
        }

        let frame_len = (buffer.len() as u32).to_be_bytes();
        for (_, clients) in &target_clients {
            clients.lock().unwrap().retain_mut(|stream| {
                stream.write_all(&frame_len).and_then(|_| stream.write_all(&buffer)).is_ok()
            });
        }
        next_packet_id = next_packet_id.saturating_add(1);

        let elapsed = cycle_start.elapsed();
        if elapsed < publish_interval {
            thread::sleep(publish_interval - elapsed);
        }
    }
}

fn main() -> Result<()> {
    let publish_hz = std::env::var("CAND_PUBLISH_HZ")
        .ok()
        .and_then(|value| value.parse::<u64>().ok())
        .filter(|value| *value > 0)
        .unwrap_or(DEFAULT_PUBLISH_HZ);

    let config_json = load_can_config_json()?;
    let packets: Vec<PacketConfig> = serde_json::from_str(&config_json)?;
    let packet_map: HashMap<u32, PacketConfig> = packets.into_iter().map(|packet| (packet.packet_id, packet)).collect();

    let sensor_data = Arc::new(Mutex::new(OrionSensorData::default()));
    let (raw_tx, raw_rx): (Sender<RawCanMessage>, Receiver<RawCanMessage>) = mpsc::channel();

    let tx0 = raw_tx.clone();
    thread::spawn(move || {
        if let Err(error) = can_reader_loop(tx0, MOCK_PORT_0) {
            eprintln!("[mock_main] can0 reader error: {error:?}");
        }
    });

    let tx1 = raw_tx.clone();
    thread::spawn(move || {
        if let Err(error) = can_reader_loop(tx1, MOCK_PORT_1) {
            eprintln!("[mock_main] can1 reader error: {error:?}");
        }
    });

    let processing_sensor_data = Arc::clone(&sensor_data);
    thread::spawn(move || {
        while let Ok(message) = raw_rx.recv() {
            if let Some((config, series_index)) = resolve_packet_config(&packet_map, message.id) {
                let mut locked = processing_sensor_data.lock().unwrap();
                process_raw_data(&mut locked, &message.payload, config, series_index);
            }
        }
    });

    let publish_targets = selected_publish_targets();
    if publish_targets.is_empty() {
        eprintln!("[mock_main] no publish targets selected; set CAND_MOCK_PUBLISH_TARGETS or leave it unset");
    }

    let publish_sensor_data = Arc::clone(&sensor_data);
    thread::spawn(move || {
        if let Err(error) = ipc_publish_loop(publish_sensor_data, publish_targets, publish_hz) {
            eprintln!("[mock_main] ipc publish error: {error:?}");
        }
    });

    println!(
        "[mock_main] listening on {} and {} and publishing to IPC sockets",
        MOCK_PORT_0, MOCK_PORT_1
    );

    loop {
        thread::park();
    }
}
