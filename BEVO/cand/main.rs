use anyhow::Result;
use prost::Message;
use socketcan::{CanSocket, Socket, EmbeddedFrame, Id};
use std::collections::HashMap;
use std::io::{BufRead, BufReader, Write};
use std::net::{TcpListener, UdpSocket};
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

//use std::process::Command;

const MOCK_ADDR: &str = "127.0.0.1:5005";
const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const STARTUP_SEMAPHORE_PATH: &str = "/tmp/BEVO_publishd_ready";
//const OTA_SEMAPHORE_PATH: &str = "/tmp/BEVO_ota_request"; 
const CAN_INTERFACE: &str = "can0";
const DEFAULT_PUBLISH_HZ: u64 = 100;
const DEFAULT_NMEA_LISTEN_ADDR: &str = "0.0.0.0:2000";
//const OTA_DOWNLOAD_START_ADDRESS: &str = "0x00000000";



const DEFAULT_CAN_JSON_PATH: &str = "BEVO/nonhermetic/assets/can.json";

#[derive(Debug)]
struct RawCanMessage {
    id: u32,
    payload: Vec<u8>,
}

#[derive(Debug, Clone, Default)]
struct ExternalDynamicsData {
    gps: Vec<f32>,
    gps_imu: Vec<f32>,
}

fn main() -> Result<()> {
    //let _ = std::fs::remove_file(OTA_SEMAPHORE_PATH);
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
    let external_dynamics = Arc::new(Mutex::new(ExternalDynamicsData::default()));
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

    let nmea_external_dynamics = Arc::clone(&external_dynamics);
    let nmea_listen_addr = std::env::var("CAND_NMEA_LISTEN_ADDR")
        .unwrap_or_else(|_| DEFAULT_NMEA_LISTEN_ADDR.to_string());
    thread::spawn(move || {
        if let Err(e) = nmea_listener_loop(nmea_external_dynamics, nmea_listen_addr) {
            eprintln!("[CAND-NMEA] Error: {:?}", e);
        }
    });

    let ipc_sensor_data = Arc::clone(&sensor_data_cache);
    let ipc_external_dynamics = Arc::clone(&external_dynamics);
    thread::spawn(move || {
        if let Err(e) = ipc_server_loop(ipc_sensor_data, ipc_external_dynamics, publish_hz, initial_packet_id) {
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

// waits for matt semaphore to be set

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

// check for OTA sempahore file and read current id and link

// fn check_for_ota() -> Option<(u32, String)> {
//     if Path::new(OTA_SEMAPHORE_PATH).exists() {
//         if let Ok(contents) = std::fs::read_to_string(OTA_SEMAPHORE_PATH) {
//             let trimmed = contents.trim();
//             let mut parts = trimmed.splitn(2, ':');
//             if let (Some(device_id_raw), Some(data_link_raw)) = (parts.next(), parts.next()) {
//                 if let Ok(device_id) = device_id_raw.parse::<u32>() {
//                     let data_link = data_link_raw.trim().to_string();
//                     if !data_link.is_empty() {
//                         return Some((device_id, data_link));
//                     }
//                 }
//             }
//         }
//     }
//     None
// }

// fn remove_ota_semaphore() {
//     if let Err(error) = std::fs::remove_file(OTA_SEMAPHORE_PATH) {
//         if error.kind() != std::io::ErrorKind::NotFound {
//             eprintln!("[CAND] failed to remove OTA semaphore: {:?}", error);
//         }
//     }
// }

// fn resolve_bevo_root() -> Result<PathBuf> {
//     if let Ok(root) = std::env::var("BEVO_ROOT") {
//         let path = PathBuf::from(root);
//         if path.join("cand/bootload.py").exists() && path.join("requirements.txt").exists() {
//             return Ok(path);
//         }
//     }

//     let cwd = std::env::current_dir()?;
//     let direct_bootload = cwd.join("cand/bootload.py");
//     let nested_bootload = cwd.join("BEVO/cand/bootload.py");

//     if direct_bootload.exists() && cwd.join("requirements.txt").exists() {
//         return Ok(cwd);
//     }
//     if nested_bootload.exists() && cwd.join("BEVO/requirements.txt").exists() {
//         return Ok(cwd.join("BEVO"));
//     }

//     anyhow::bail!("unable to resolve BEVO root; set BEVO_ROOT env var")
// }

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

fn nmea_listener_loop(external_dynamics: Arc<Mutex<ExternalDynamicsData>>, listen_addr: String) -> Result<()> {
    let listener = TcpListener::bind(&listen_addr)?;
    println!("[CAND-NMEA] Listening on {}", listen_addr);

    for incoming in listener.incoming() {
        match incoming {
            Ok(stream) => {
                let peer = stream.peer_addr().ok();
                println!("[CAND-NMEA] Client connected: {:?}", peer);
                let reader = BufReader::new(stream);
                for line in reader.lines() {
                    let sentence = match line {
                        Ok(v) => v,
                        Err(e) => {
                            eprintln!("[CAND-NMEA] Read error: {:?}", e);
                            break;
                        }
                    };
                    update_external_dynamics_from_sentence(&external_dynamics, sentence.trim());
                }
            }
            Err(e) => {
                eprintln!("[CAND-NMEA] Accept error: {:?}", e);
            }
        }
    }
    Ok(())
}

fn update_external_dynamics_from_sentence(external_dynamics: &Arc<Mutex<ExternalDynamicsData>>, sentence: &str) {
    if sentence.is_empty() {
        return;
    }

    if let Some((lat, lon)) = parse_nmea_gps(sentence) {
        let mut locked = external_dynamics.lock().unwrap();
        locked.gps.clear();
        locked.gps.push(lat);
        locked.gps.push(lon);
        return;
    }

    if let Some((x, y, z)) = parse_pimu_xyz(sentence) {
        let mut locked = external_dynamics.lock().unwrap();
        locked.gps_imu.clear();
        locked.gps_imu.push(x);
        locked.gps_imu.push(y);
        locked.gps_imu.push(z);
    }
}

fn parse_nmea_gps(sentence: &str) -> Option<(f32, f32)> {
    if !sentence.starts_with('$') {
        return None;
    }
    let payload = sentence.split('*').next()?;
    let fields: Vec<&str> = payload.split(',').collect();
    let msg = fields.first()?;

    let (lat_raw, lat_hemi, lon_raw, lon_hemi) = if msg.ends_with("GGA") {
        (
            *fields.get(2)?,
            *fields.get(3)?,
            *fields.get(4)?,
            *fields.get(5)?,
        )
    } else if msg.ends_with("RMC") {
        (
            *fields.get(3)?,
            *fields.get(4)?,
            *fields.get(5)?,
            *fields.get(6)?,
        )
    } else {
        return None;
    };

    let lat = nmea_coord_to_decimal(lat_raw, lat_hemi)?;
    let lon = nmea_coord_to_decimal(lon_raw, lon_hemi)?;
    Some((lat, lon))
}

fn nmea_coord_to_decimal(raw: &str, hemi: &str) -> Option<f32> {
    let raw = raw.trim();
    if raw.is_empty() {
        return None;
    }
    let value: f64 = raw.parse().ok()?;
    let degrees = (value / 100.0).floor();
    let minutes = value - (degrees * 100.0);
    let mut decimal = degrees + minutes / 60.0;

    let hemi_upper = hemi.trim().to_ascii_uppercase();
    if hemi_upper == "S" || hemi_upper == "W" {
        decimal *= -1.0;
    }
    Some(decimal as f32)
}

fn parse_pimu_xyz(sentence: &str) -> Option<(f32, f32, f32)> {
    if !sentence.starts_with("$PIMU") {
        return None;
    }
    let payload = sentence.split('*').next()?;
    let mut parts = payload.split(',');
    parts.next()?;
    let x: f32 = parts.next()?.trim().parse().ok()?;
    let y: f32 = parts.next()?.trim().parse().ok()?;
    let z: f32 = parts.next()?.trim().parse().ok()?;
    Some((x, y, z))
}

fn apply_external_dynamics(data: &mut OrionSensorData, external_dynamics: &ExternalDynamicsData) {
    let dynamics = data.dynamics.get_or_insert_with(Default::default);
    dynamics.gps = external_dynamics.gps.clone();
    dynamics.gps_imu = external_dynamics.gps_imu.clone();
}

// fn ota_processing_loop(device_id: u32, data_link: String) -> Result<std::path::PathBuf> {
//     // create a temporary file path that includes the device id so multiple
//     // OTA downloads don't collide
//     let mut tmp = std::env::temp_dir();
//     tmp.push(format!("bevo_ota_{}.bin", device_id));

//     let curl_status = Command::new("curl")
//         .arg("-fL")
//         .arg("--retry")
//         .arg("3")
//         .arg("--connect-timeout")
//         .arg("10")
//         .arg("--max-time")
//         .arg("120")
//         .arg("--output")
//         .arg(&tmp)
//         .arg(&data_link)
//         .status()?;
//     if !curl_status.success() {
//         anyhow::bail!("curl download failed with status {}", curl_status);
//     }

//     let bevo_root = resolve_bevo_root()?;

//     // ensure Python virtualenv exists in BEVO root
//     let venv_dir = bevo_root.join("venv");
//     let python_exe = if cfg!(windows) {
//         venv_dir.join("Scripts/python.exe")
//     } else {
//         venv_dir.join("bin/python3")
//     };

//     if !python_exe.exists() {
//         println!("[CAND] creating python venv at {}", venv_dir.display());
//         let status = Command::new("python3")
//             .arg("-m")
//             .arg("venv")
//             .arg(&venv_dir)
//             .status()?;
//         if !status.success() {
//             anyhow::bail!("failed to create venv, exit status {}", status);
//         }

//         let status = Command::new(&python_exe)
//             .arg("-m")
//             .arg("pip")
//             .arg("install")
//             .arg("-r")
//             .arg(bevo_root.join("requirements.txt"))
//             .status()?;
//         if !status.success() {
//             anyhow::bail!("failed to install python requirements, exit status {}", status);
//         }
//     }

//     // call bootload script with firmware path, flash start address, and device id
//     let boot_script = bevo_root.join("cand/bootload.py");
//     let status = Command::new(&python_exe)
//         .arg(&boot_script)
//         .arg(&tmp)
//         .arg(OTA_DOWNLOAD_START_ADDRESS)
//         .arg(device_id.to_string())
//         .status()?;
//     if !status.success() {
//         anyhow::bail!("bootload.py failed with status {}", status);
//     }

//     Ok(tmp)
// }

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

fn ipc_server_loop(
    sensor_data: Arc<Mutex<OrionSensorData>>,
    external_dynamics: Arc<Mutex<ExternalDynamicsData>>,
    publish_hz: u64,
    initial_packet_id: u64,
) -> Result<()> {
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

        // check OTA smeaphore and divert to downloading release and sending firmware update packets

        // if let Some((device_id, link)) = check_for_ota() {
        //     println!(
        //         "[CAND] OTA request detected for device {} -> {}",
        //         device_id, link
        //     );
        //     match ota_processing_loop(device_id, link.clone()) {
        //         Ok(path) => {
        //             println!("[CAND] OTA completed using {}", path.display());
        //             remove_ota_semaphore();
        //         }
        //         Err(e) => eprintln!("[CAND] failed to download OTA image: {:?}", e),
        //     }
        //     // after handling, sleep briefly before next iteration
        //     thread::sleep(Duration::from_secs(1));
        //     continue;
        // }

        {
            let mut data = sensor_data.lock().unwrap();
            data.packet_id = (next_packet_id.min(i64::MAX as u64)) as i64;
            data.time = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_millis() as i64;
            let external_snapshot = external_dynamics.lock().unwrap().clone();
            apply_external_dynamics(&mut data, &external_snapshot);
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