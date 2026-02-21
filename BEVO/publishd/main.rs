use anyhow::Result;
use rumqttc::{Client, Event, Incoming, MqttOptions, QoS};
use serde_json::Value;
use std::io::Read;
use std::os::unix::net::UnixStream;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, SyncSender, TrySendError};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

const IPC_SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const STARTUP_SEMAPHORE_PATH: &str = "/tmp/BEVO_publishd_ready";
const MQTT_HOST: &str = "192.168.1.109";
const MQTT_PORT: u16 = 1883;
const MQTT_CLIENT_ID: &str = "BEVO-ORION";
const MQTT_TOPIC_PUBLISH: &str = "orion";
const MQTT_TOPIC_SERVER_COMMUNICATION: &str = "server-communication";
const MQTT_OUTBOUND_QUEUE_CAPACITY: usize = 2048;

struct MqttClient {
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

fn main() -> Result<()> {
    let _ = std::fs::remove_file(STARTUP_SEMAPHORE_PATH);
    let mqtt_client = MqttClient::new(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID)?;
    println!(
        "publishd connected to mqtt broker {}:{} with client_id {}",
        MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID
    );

    let initial_packet_id = mqtt_client.wait_for_initial_packet_id();
    write_startup_semaphore(initial_packet_id)?;
    println!("publishd startup complete, initial packet_id={}", initial_packet_id);

    loop {
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
