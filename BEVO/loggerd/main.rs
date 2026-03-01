use anyhow::Result;
use prost::Message;
use serde_json::Value;
use std::collections::BTreeMap;
use std::fs::{File, OpenOptions};
use std::io::Read;
use std::io::BufWriter;
use std::io::Write;
use std::os::unix::net::UnixStream;
use std::path::PathBuf;
use std::sync::mpsc::{self, SyncSender, TrySendError};
use std::thread;
use std::time::SystemTime;
use std::time::Duration;

use sensor_proto::proto::orion::OrionSensorData;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";
const DEFAULT_LOG_DIR: &str = "BEVO/loggerd/logs";
const LOGGER_ENABLED: bool = true;
const LOGGER_QUEUE_CAPACITY: usize = 4096;
const LOGGER_FLUSH_EVERY_ROWS: usize = 100;

struct CsvLogger {
    writer: BufWriter<File>,
    headers: Vec<String>,
    rows_since_flush: usize,
}

impl CsvLogger {
    fn new(csv_path: String) -> Result<Self> {
        let file = OpenOptions::new()
            .create(true)
            .write(true)
            .truncate(true)
            .open(&csv_path)?;
        let writer = BufWriter::new(file);

        Ok(Self {
            writer,
            headers: Vec::new(),
            rows_since_flush: 0,
        })
    }

    fn log_message(&mut self, data: &OrionSensorData) -> Result<()> {
        let value = serde_json::to_value(data)?;
        let mut row = BTreeMap::new();
        flatten_json_value("", &value, &mut row);

        if self.headers.is_empty() {
            self.headers = row.keys().cloned().collect();
            write_csv_record(&mut self.writer, self.headers.iter().map(|s| s.as_str()))?;
        }

        let values = self
            .headers
            .iter()
            .map(|header| row.get(header).map_or("", String::as_str));
        write_csv_record(&mut self.writer, values)?;

        self.rows_since_flush += 1;
        if self.rows_since_flush >= LOGGER_FLUSH_EVERY_ROWS {
            self.writer.flush()?;
            self.rows_since_flush = 0;
        }

        Ok(())
    }
}

fn flatten_json_value(prefix: &str, value: &Value, output: &mut BTreeMap<String, String>) {
    match value {
        Value::Object(map) => {
            for (key, child) in map {
                let next_prefix = if prefix.is_empty() {
                    key.to_string()
                } else {
                    format!("{}.{}", prefix, key)
                };
                flatten_json_value(&next_prefix, child, output);
            }
        }
        Value::Array(items) => {
            for (index, child) in items.iter().enumerate() {
                let next_prefix = format!("{}[{}]", prefix, index);
                flatten_json_value(&next_prefix, child, output);
            }
        }
        Value::Null => {
            if !prefix.is_empty() {
                output.insert(prefix.to_string(), String::new());
            }
        }
        Value::Bool(boolean_value) => {
            output.insert(prefix.to_string(), boolean_value.to_string());
        }
        Value::Number(number_value) => {
            output.insert(prefix.to_string(), number_value.to_string());
        }
        Value::String(string_value) => {
            output.insert(prefix.to_string(), string_value.clone());
        }
    }
}

fn write_csv_record<'a>(writer: &mut BufWriter<File>, fields: impl Iterator<Item = &'a str>) -> Result<()> {
    let mut first = true;
    for field in fields {
        if !first {
            writer.write_all(b",")?;
        }
        first = false;
        let escaped = csv_escape(field);
        writer.write_all(escaped.as_bytes())?;
    }
    writer.write_all(b"\n")?;
    Ok(())
}

fn csv_escape(value: &str) -> String {
    let needs_quotes = value.contains(',') || value.contains('"') || value.contains('\n');
    if !needs_quotes {
        return value.to_string();
    }

    let escaped = value.replace('"', "\"\"");
    format!("\"{}\"", escaped)
}

fn default_log_file_path() -> Result<String> {
    let workspace_root = std::env::var("BUILD_WORKSPACE_DIRECTORY")
        .ok()
        .map(PathBuf::from)
        .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")));

    let mut log_dir_path = workspace_root;
    log_dir_path.push(DEFAULT_LOG_DIR);
    std::fs::create_dir_all(&log_dir_path)?;

    let timestamp_ms = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap_or(Duration::from_secs(0))
        .as_millis();
    let file_name = format!("orion_{}.csv", timestamp_ms);
    log_dir_path.push(file_name);

    Ok(log_dir_path.to_string_lossy().to_string())
}

fn spawn_logger_thread(csv_path: String) -> SyncSender<OrionSensorData> {
    let (tx, rx) = mpsc::sync_channel::<OrionSensorData>(LOGGER_QUEUE_CAPACITY);

    thread::spawn(move || {
        let mut logger = match CsvLogger::new(csv_path) {
            Ok(logger) => logger,
            Err(error) => {
                eprintln!("[LOGGERD] Failed to initialize CSV logger: {error}");
                return;
            }
        };

        while let Ok(data) = rx.recv() {
            if let Err(error) = logger.log_message(&data) {
                eprintln!("[LOGGERD] CSV write error: {error}");
            }
        }

        if let Err(error) = logger.writer.flush() {
            eprintln!("[LOGGERD] CSV flush error: {error}");
        }
    });

    tx
}

fn main() -> Result<()> {
    if !LOGGER_ENABLED {
        return Ok(());
    }

    let logger_tx = if LOGGER_ENABLED {
        let csv_path = default_log_file_path()?;
        Some(spawn_logger_thread(csv_path))
    } else {
        None
    };

    loop {
        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                loop {
                    let mut length_buffer = [0u8; 4];
                    if stream.read_exact(&mut length_buffer).is_err() {
                        break;
                    }

                    let message_length = u32::from_be_bytes(length_buffer) as usize;
                    let mut message_buffer = vec![0u8; message_length];
                    if let Err(e) = stream.read_exact(&mut message_buffer) {
                        eprintln!("[LOGGERD] Read error: {}", e);
                        break;
                    }

                    if !LOGGER_ENABLED {
                        continue;
                    }

                    match OrionSensorData::decode(&message_buffer[..]) {
                        Ok(data) => {
                            if let Some(tx) = &logger_tx {
                                match tx.try_send(data) {
                                    Ok(_) => {}
                                    Err(TrySendError::Full(_)) => {
                                        eprintln!("[LOGGERD] Logger queue full; dropping frame");
                                    }
                                    Err(TrySendError::Disconnected(_)) => {
                                        eprintln!("[LOGGERD] Logger queue disconnected");
                                    }
                                }
                            }
                        }
                        Err(e) => eprintln!("[LOGGERD] Decode error: {}", e),
                    }
                }
            }
            Err(_) => {
                thread::sleep(Duration::from_secs(2));
            }
        }
    }
}