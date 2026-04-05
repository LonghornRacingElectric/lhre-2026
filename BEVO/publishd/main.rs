use anyhow::Result;
use rumqttc::{Client, Event, Incoming, MqttOptions, QoS};
use serde_json::Value;
use std::io::Read;
use std::os::unix::net::UnixStream;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, SyncSender, TrySendError};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

// OTA globals
// static OTA_LINK: Mutex<Option<String>> = Mutex::new(None);
// static OTA_DEVICE_ID: Mutex<Option<u32>> = Mutex::new(None);

const IPC_SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const STARTUP_SEMAPHORE_PATH: &str = "/tmp/BEVO_publishd_ready";
// const OTA_SEMAPHORE_PATH: &str = "/tmp/BEVO_ota_request";
// // const MQTT_HOST: &str = "192.168.1.109";
const MQTT_HOST: &str = "18.191.225.118"; // aws broker hard coded for testing
const MQTT_PORT: u16 = 1883;
const MQTT_CLIENT_ID: &str = "BEVO-ORION";
const MQTT_ANNOUNCE_CLIENT_ID: &str = "BEVO-Orion";
const MQTT_TOPIC_PUBLISH: &str = "orion";
const MQTT_TOPIC_SERVER_COMMUNICATION: &str = "server-communication";
const MQTT_TOPIC_CLIENT_CONNECTIONS: &str = "client-connections";
const MQTT_OUTBOUND_QUEUE_CAPACITY: usize = 2048;
const INITIAL_PACKET_ID_REQUEST_TIMEOUT_SECS: u64 = 12;
const INITIAL_PACKET_ID_REQUEST_INTERVAL_MS: u64 = 500;

fn env_or_default(name: &str, default: &str) -> String {
    std::env::var(name).unwrap_or_else(|_| default.to_string())
}

fn should_require_server_packet_id() -> bool {
    matches!(
        std::env::var("PUBLISHD_REQUIRE_SERVER_PACKET_ID")
            .ok()
            .as_deref()
            .map(|value| value.to_ascii_lowercase()),
        Some(value) if value == "1" || value == "true" || value == "yes"
    )
}

fn packet_id_announce_payload(client_id: &str) -> String {
    env_or_default("PUBLISHD_CLIENT_ANNOUNCE_ID", client_id)
}

struct MqttClient {
    client: Arc<Mutex<Client>>,
    outbound_tx: SyncSender<Vec<u8>>,
    packet_id: Arc<AtomicU64>,
    initialized: Arc<std::sync::atomic::AtomicBool>,
}

impl MqttClient {
    fn new(host: &str, port: u16, client_id: &str) -> Result<Self> {
        let mut mqtt_options = MqttOptions::new(client_id, host, port);
        mqtt_options.set_keep_alive(Duration::from_secs(20));

        let (client, mut connection) = Client::new(mqtt_options, 64);
        let client = Arc::new(Mutex::new(client));
        let packet_id = Arc::new(AtomicU64::new(0));
        let initialized = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let (outbound_tx, outbound_rx) = mpsc::sync_channel::<Vec<u8>>(MQTT_OUTBOUND_QUEUE_CAPACITY);

        {
            let lock = client.lock();
            if let Ok(locked_client) = lock {
                locked_client.subscribe(MQTT_TOPIC_SERVER_COMMUNICATION, QoS::AtMostOnce)?;
            } else {
                anyhow::bail!("mqtt client mutex poisoned during subscribe");
            }
        }

        let publish_client = Arc::clone(&client);
        thread::spawn(move || {
            while let Ok(payload) = outbound_rx.recv() {
                let publish_result = publish_client
                    .lock()
                    .expect("mqtt client mutex poisoned")
                    .publish(MQTT_TOPIC_PUBLISH, QoS::AtMostOnce, false, payload);

                if let Err(error) = publish_result {
                    eprintln!("publishd mqtt publish error: {error}");
                }
            }
        });

        let packet_id_for_loop = Arc::clone(&packet_id);
        let initialized_for_loop = Arc::clone(&initialized);
        thread::spawn(move || {
            for event in connection.iter() {
                match event {
                    Ok(Event::Incoming(Incoming::Publish(publish))) => {
                        if publish.topic == MQTT_TOPIC_SERVER_COMMUNICATION {
                            handle_server_message(&packet_id_for_loop, &initialized_for_loop, &publish.payload);
                        }
                    }
                    Ok(Event::Incoming(Incoming::Disconnect)) => break,
                    Ok(_) => {}
                    Err(error) => {
                        eprintln!("publishd mqtt connection error: {error}");
                        thread::sleep(Duration::from_millis(200));
                    }
                }
            }
        });

        Ok(Self {
            client,
            outbound_tx,
            packet_id,
            initialized,
        })
    }

    fn publish_sensor_bytes(&self, payload: &[u8]) -> Result<()> {
        match self.outbound_tx.try_send(payload.to_vec()) {
            Ok(_) => Ok(()),
            Err(TrySendError::Full(_)) => {
                eprintln!("publishd outbound queue full; dropping frame");
                Ok(())
            }
            Err(TrySendError::Disconnected(_)) => anyhow::bail!("publishd outbound queue disconnected"),
        }
    }

    fn packet_id(&self) -> u64 {
        self.packet_id.load(Ordering::Relaxed)
    }

    fn wait_for_initial_packet_id(&self) -> u64 {
        while !self.initialized.load(Ordering::Relaxed) {
            thread::sleep(Duration::from_millis(100));
        }
        self.packet_id()
    }

    fn request_initial_packet_id_or_default(&self, announce_payload: &str, default: u64) -> u64 {
        let deadline = Instant::now() + Duration::from_secs(INITIAL_PACKET_ID_REQUEST_TIMEOUT_SECS);
        while Instant::now() < deadline {
            if self.initialized.load(Ordering::Relaxed) {
                return self.packet_id();
            }

            println!(
                "publishd requesting packet_id on topic '{}' with payload '{}'",
                MQTT_TOPIC_CLIENT_CONNECTIONS,
                announce_payload
            );

            let publish_result = self
                .client
                .lock()
                .expect("mqtt client mutex poisoned")
                .publish(
                    MQTT_TOPIC_CLIENT_CONNECTIONS,
                    QoS::AtMostOnce,
                    false,
                    announce_payload.as_bytes().to_vec(),
                );
            if let Err(error) = publish_result {
                eprintln!("publishd mqtt packet_id request publish error: {error}");
            }

            thread::sleep(Duration::from_millis(INITIAL_PACKET_ID_REQUEST_INTERVAL_MS));
        }

        eprintln!(
            "publishd did not receive server packet_id within {}s; defaulting to {}",
            INITIAL_PACKET_ID_REQUEST_TIMEOUT_SECS,
            default
        );
        default
    }
}

fn handle_server_message(
    packet_id: &Arc<AtomicU64>,
    initialized: &Arc<std::sync::atomic::AtomicBool>,
    payload: &[u8],
) {
    let parsed: Result<Value, _> = serde_json::from_slice(payload);
    let Ok(message) = parsed else {
        return;
    };

    // if message.get("type").and_then(Value::as_str).is_some()
    // {
    //     println!("publishd received server message: {}", message);
    // }

    // if message.get("packet_id").is_none() {
    //     // Assume request for OTA
    //     if let Some(ota_obj) = message.get("OTA_request").and_then(Value::as_object) {
    //         if let Some(link) = ota_obj.get("OTA_link").and_then(Value::as_str) {
    //             if let Some(device_id) = ota_obj.get("OTA_device_id").and_then(Value::as_u64) {
    //                 let mut ota_link_lock = OTA_LINK.lock().unwrap();
    //                 *ota_link_lock = Some(link.to_string());
    //                 println!("publishd received OTA link: {}", link);
    //                 let mut ota_device_id_lock = OTA_DEVICE_ID.lock().unwrap();
    //                 *ota_device_id_lock = Some(device_id as u32);
    //                 println!("publishd received OTA device ID: {}", device_id);

    //                 // set semaphore to tell can daemon to transition to sending packets rather than sniffing CAN bus
    //                 write_ota_semaphore(device_id as u32, link).unwrap_or_else(|error| {
    //                     eprintln!("publishd failed to write OTA semaphore: {error}");
    //                 });
    //             } else {
    //                 eprintln!("publishd received malformed OTA request: {}", message);
    //             }
    //         } else {
    //             eprintln!("publishd received malformed OTA request: {}", message);
    //         }
    //     }
    // }

    if let Some(server_packet_id) = message.get("packet_id").and_then(Value::as_u64) {
        let candidate = server_packet_id.saturating_add(1);
        loop {
            let current = packet_id.load(Ordering::Relaxed);
            if candidate <= current {
                break;
            }
            if packet_id
                .compare_exchange(current, candidate, Ordering::Relaxed, Ordering::Relaxed)
                .is_ok()
            {
                initialized.store(true, Ordering::Relaxed);
                println!("publishd updated packet_id to {candidate} from server-communication");
                break;
            }
        }
    }
}

fn write_startup_semaphore(packet_id: u64) -> Result<()> {
    let temp_path = format!("{}.tmp", STARTUP_SEMAPHORE_PATH);
    std::fs::write(&temp_path, packet_id.to_string())?;
    std::fs::rename(temp_path, STARTUP_SEMAPHORE_PATH)?;
    Ok(())
}

// fn write_ota_semaphore(device_id: u32, data_link: &str) -> Result<()> {
//     let temp_path = format!("{}.tmp", OTA_SEMAPHORE_PATH);
//     std::fs::write(&temp_path, format!("{}:{}", device_id, data_link))?;
//     std::fs::rename(temp_path, OTA_SEMAPHORE_PATH)?;
//     Ok(())
// }

fn main() -> Result<()> {
    let _ = std::fs::remove_file(STARTUP_SEMAPHORE_PATH);
    // let _ = std::fs::remove_file(OTA_SEMAPHORE_PATH);
    let mqtt_host = env_or_default("PUBLISHD_MQTT_HOST", MQTT_HOST);
    let mqtt_port = std::env::var("PUBLISHD_MQTT_PORT")
        .ok()
        .and_then(|value| value.parse::<u16>().ok())
        .unwrap_or(MQTT_PORT);
    let mqtt_client_id = env_or_default("PUBLISHD_MQTT_CLIENT_ID", MQTT_CLIENT_ID);
    let announce_payload = packet_id_announce_payload(MQTT_ANNOUNCE_CLIENT_ID);

    let mqtt_client = MqttClient::new(&mqtt_host, mqtt_port, &mqtt_client_id)?;
    println!(
        "publishd connected to mqtt broker {}:{} with client_id {}",
        mqtt_host, mqtt_port, mqtt_client_id
    );

    let require_server_packet_id = should_require_server_packet_id();
    println!(
        "publishd require_server_packet_id={} (set PUBLISHD_REQUIRE_SERVER_PACKET_ID=1 to enable handshake)",
        require_server_packet_id
    );

    let initial_packet_id = if require_server_packet_id {
        println!(
            "publishd requesting initial packet_id using client-connections payload '{}'",
            announce_payload
        );
        mqtt_client.request_initial_packet_id_or_default(&announce_payload, 1)
    } else {
        1
    };
    write_startup_semaphore(initial_packet_id)?;
    println!("publishd startup complete, initial packet_id={}", initial_packet_id);

    loop {
        // check if there is incoming message from the server
        match UnixStream::connect(IPC_SOCKET_PATH) {
            Ok(mut stream) => loop {
                let mut length_buffer = [0u8; 4];
                if stream.read_exact(&mut length_buffer).is_err() {
                    break;
                }

                let message_length = u32::from_be_bytes(length_buffer) as usize;
                let mut message_buffer = vec![0u8; message_length];
                if stream.read_exact(&mut message_buffer).is_err() {
                    break;
                }

                if let Err(error) = mqtt_client.publish_sensor_bytes(&message_buffer) {
                    eprintln!("publishd mqtt publish error: {error}");
                }
            },
            Err(_) => {
                let _ = mqtt_client.packet_id();
                thread::sleep(Duration::from_millis(500));
            }
        }
    }
}
