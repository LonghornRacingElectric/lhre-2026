use anyhow::Result;
use prost::Message;
use serde_json::Value;
use std::collections::{BTreeMap, HashMap};
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

use sensor_proto::proto::orion::{
    BoardStatus, Controls, DiagnosticsHigh, DiagnosticsLow, Dynamics, OrionSensorData, Pack,
    Thermal,
};
use sensor_proto::config::PacketConfig;

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
        let mut writer = BufWriter::new(file);

        // Pre-populate the header from the full proto schema rather than the
        // first-seen row. Without this, if the first packet has any nested
        // sub-message as None, flatten_json_value emits a bare top-level key
        // (e.g. "pack") instead of the per-field keys (e.g. "pack.hv_pack_v")
        // that later populated packets produce. The header would lock to the
        // bare form and silently drop every nested value thereafter.
        let headers = canonical_csv_headers();
        write_csv_record(&mut writer, headers.iter().map(|s| s.as_str()))?;

        Ok(Self {
            writer,
            headers,
            rows_since_flush: 0,
        })
    }

    fn log_message(&mut self, data: &OrionSensorData) -> Result<()> {
        let value = serde_json::to_value(data)?;
        let mut row = BTreeMap::new();
        flatten_json_value("", &value, &mut row);

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

/// Resolve the BEVO workspace root the same way the rest of the daemon does:
/// the Bazel-provided workspace dir if present, else the current directory.
fn workspace_root() -> PathBuf {
    std::env::var("BUILD_WORKSPACE_DIRECTORY")
        .ok()
        .map(PathBuf::from)
        .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
}

/// Path to the generated CAN schema (`can.json`). Mirrors cand's resolution so
/// loggerd reads the same artifact the rest of BEVO does. Override with
/// LOGGERD_CAN_JSON_PATH for tests or non-standard deployments.
fn can_json_path() -> PathBuf {
    if let Ok(path) = std::env::var("LOGGERD_CAN_JSON_PATH") {
        return PathBuf::from(path);
    }
    let mut path = workspace_root();
    path.push("BEVO/nonhermetic/assets/can.json");
    path
}

fn load_packet_configs() -> Result<Vec<PacketConfig>> {
    let path = can_json_path();
    let contents = std::fs::read_to_string(&path)
        .map_err(|error| anyhow::anyhow!("reading {}: {error}", path.display()))?;
    let packets: Vec<PacketConfig> = serde_json::from_str(&contents)?;
    Ok(packets)
}

/// Fallback lengths used only when can.json can't be read, so the logger still
/// emits a sensible header if the asset is missing. Mirrors the current Orion
/// schema (cells_v = 33 frames x 4 = 132, cells_temps = 23 x 4 = 92, accel/gyro
/// vectors = 3 axes each).
const DEFAULT_REPEATED_LENGTHS: &[(&str, usize)] = &[
    ("cells_v", 132),
    ("cells_temps", 92),
    ("bl_unsprung_accel", 3),
    ("br_unsprung_accel", 3),
    ("fl_unsprung_accel", 3),
    ("fr_unsprung_accel", 3),
    ("bl_sprung_accel", 3),
    ("br_sprung_accel", 3),
    ("fl_sprung_accel", 3),
    ("fr_sprung_accel", 3),
    ("bl_gyro", 3),
    ("br_gyro", 3),
    ("fl_gyro", 3),
    ("fr_gyro", 3),
];

/// Derive the length of every repeated proto field straight from the generated
/// CAN schema, so the CSV header is sized from the artifact instead of magic
/// numbers. A repeated field spans `quantity * cells_per_frame`, where
/// `cells_per_frame` is the number of slots packed into one CAN frame
/// (max field_index + 1). e.g. Cell Voltages = 33 frames x 4 cells = 132.
///
/// `gps` / `gps_imu` aren't described in can.json (cand fills them from the NMEA
/// stream), so they're supplied as constants.
fn repeated_field_lengths() -> HashMap<String, usize> {
    let mut lengths: HashMap<String, usize> = HashMap::new();

    match load_packet_configs() {
        Ok(packets) => {
            for packet in &packets {
                let quantity = packet.quantity.max(1) as usize;
                // cells-per-frame is tracked per repeated field within this
                // packet, then multiplied by how many frames the series has.
                let mut cells_per_frame: HashMap<&str, usize> = HashMap::new();
                for signal in &packet.bytes {
                    if let Some(pb) = &signal.protobuf {
                        if pb.repeated {
                            if let Some(index) = pb.field_index {
                                let span =
                                    cells_per_frame.entry(pb.field_name.as_str()).or_insert(0);
                                *span = (*span).max(index + 1);
                            }
                        }
                    }
                }
                for (name, span) in cells_per_frame {
                    let total = quantity * span;
                    let entry = lengths.entry(name.to_string()).or_insert(0);
                    *entry = (*entry).max(total);
                }
            }
        }
        Err(error) => {
            eprintln!(
                "[LOGGERD] Could not load CAN schema for CSV sizing ({error}); \
                 falling back to built-in defaults"
            );
            for (field, len) in DEFAULT_REPEATED_LENGTHS {
                lengths.insert((*field).to_string(), *len);
            }
        }
    }

    // NMEA-populated fields that don't appear in can.json.
    lengths.entry("gps".to_string()).or_insert(2);
    lengths.entry("gps_imu".to_string()).or_insert(3);

    lengths
}

fn canonical_csv_headers() -> Vec<String> {
    // Build a fully-populated OrionSensorData template (all sub-messages present
    // as Some(default)) so flatten_json_value enumerates every reachable scalar
    // key once. Repeated fields start as empty vecs; expand_repeated_arrays then
    // grows each to its schema-derived length so flatten emits the per-index
    // columns (e.g. `pack.cells_v[0..131]`). Without this, repeated arrays stay
    // empty at file creation, the header locks without indexed columns, and
    // every per-cell value is silently dropped from the CSV at runtime.
    let template = OrionSensorData {
        time: 0,
        packet_id: 0,
        dynamics: Some(Dynamics::default()),
        controls: Some(Controls::default()),
        pack: Some(Pack::default()),
        diagnostics_high: Some(DiagnosticsHigh::default()),
        diagnostics_low: Some(DiagnosticsLow::default()),
        thermal: Some(Thermal::default()),
        board_status: Some(BoardStatus::default()),
    };

    let mut value = serde_json::to_value(&template)
        .expect("OrionSensorData::default() must always serialize to JSON");
    let lengths = repeated_field_lengths();
    expand_repeated_arrays("", &mut value, &lengths);

    let mut row = BTreeMap::new();
    flatten_json_value("", &value, &mut row);
    order_headers(row.keys().cloned().collect())
}

/// Grow each empty repeated-field array in the serialized template to its
/// schema-derived length (looked up by the field's leaf name), filling with 0.0
/// so flatten_json_value emits one indexed column per slot.
fn expand_repeated_arrays(leaf: &str, value: &mut Value, lengths: &HashMap<String, usize>) {
    match value {
        Value::Object(map) => {
            for (key, child) in map.iter_mut() {
                expand_repeated_arrays(key, child, lengths);
            }
        }
        Value::Array(items) if items.is_empty() => {
            if let Some(&len) = lengths.get(leaf) {
                *items = vec![Value::from(0.0); len];
            }
        }
        _ => {}
    }
}

/// Order the CSV columns: `time` first, then `packet_id`, then every other
/// column in natural (numeric-aware) order so indexed arrays read
/// `pack.cells_v[0], [1], [2], ... [10], ... [100]` instead of lexical
/// `[0], [1], [10], [100], [11]`.
fn order_headers(mut keys: Vec<String>) -> Vec<String> {
    const LEADING: [&str; 2] = ["time", "packet_id"];

    // `time`, then `packet_id`, in that order, if present.
    let mut ordered: Vec<String> = LEADING
        .iter()
        .filter(|&leading| keys.iter().any(|key| key == leading))
        .map(|leading| (*leading).to_string())
        .collect();

    // Sort the remaining columns in place (no cloning) in natural order.
    keys.retain(|key| !LEADING.contains(&key.as_str()));
    keys.sort_by(|a, b| natural_cmp(a, b));

    ordered.extend(keys);
    ordered
}

/// Compare two strings treating runs of digits as numbers, so
/// `cells_v[2]` < `cells_v[10]` < `cells_v[100]`.
fn natural_cmp(a: &str, b: &str) -> std::cmp::Ordering {
    use std::cmp::Ordering;
    let mut a = a.as_bytes();
    let mut b = b.as_bytes();
    loop {
        match (a.first(), b.first()) {
            (None, None) => return Ordering::Equal,
            (None, Some(_)) => return Ordering::Less,
            (Some(_), None) => return Ordering::Greater,
            (Some(&ca), Some(&cb)) => {
                let a_digit = ca.is_ascii_digit();
                let b_digit = cb.is_ascii_digit();
                if a_digit && b_digit {
                    let (na, ra) = take_number(a);
                    let (nb, rb) = take_number(b);
                    match na.cmp(&nb) {
                        Ordering::Equal => {
                            a = ra;
                            b = rb;
                        }
                        other => return other,
                    }
                } else if a_digit != b_digit {
                    // digits sort before non-digit characters
                    return if a_digit { Ordering::Less } else { Ordering::Greater };
                } else {
                    match ca.cmp(&cb) {
                        Ordering::Equal => {
                            a = &a[1..];
                            b = &b[1..];
                        }
                        other => return other,
                    }
                }
            }
        }
    }
}

/// Parse the leading run of ASCII digits as a number, returning it with the
/// remaining bytes. Over-long runs saturate to u64::MAX (indices are tiny).
fn take_number(bytes: &[u8]) -> (u64, &[u8]) {
    let mut end = 0;
    while end < bytes.len() && bytes[end].is_ascii_digit() {
        end += 1;
    }
    let number = std::str::from_utf8(&bytes[..end])
        .ok()
        .and_then(|text| text.parse::<u64>().ok())
        .unwrap_or(u64::MAX);
    (number, &bytes[end..])
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
    let mut log_dir_path = workspace_root();
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


#[cfg(test)]
mod tests {
    use super::*;

    // Point the schema loader at this crate's checked-in can.json so the tests
    // exercise the real artifact-driven path (not just the fallback).
    fn headers() -> Vec<String> {
        std::env::set_var(
            "LOGGERD_CAN_JSON_PATH",
            concat!(env!("CARGO_MANIFEST_DIR"), "/nonhermetic/assets/can.json"),
        );
        canonical_csv_headers()
    }

    fn indexed(headers: &[String], prefix: &str) -> Vec<usize> {
        headers
            .iter()
            .filter_map(|h| h.strip_prefix(prefix))
            .filter_map(|rest| rest.strip_suffix(']'))
            .map(|n| n.parse::<usize>().expect("numeric index"))
            .collect()
    }

    #[test]
    fn time_is_first_column_then_packet_id() {
        let headers = headers();
        assert_eq!(headers[0], "time");
        assert_eq!(headers[1], "packet_id");
    }

    #[test]
    fn cells_v_sized_from_schema_and_ordered_numerically() {
        // 33 frames x 4 cells/frame = 132 (Orion: 130 real cells, last 2 unused).
        let cells = indexed(&headers(), "pack.cells_v[");
        assert_eq!(cells, (0..132).collect::<Vec<_>>());
    }

    #[test]
    fn cells_temps_sized_from_schema() {
        // 23 frames x 4 = 92.
        let temps = indexed(&headers(), "pack.cells_temps[");
        assert_eq!(temps, (0..92).collect::<Vec<_>>());
    }

    #[test]
    fn gyro_columns_present() {
        let headers = headers();
        for gyro in ["fl_gyro", "fr_gyro", "bl_gyro", "br_gyro"] {
            let key = format!("dynamics.{gyro}[0]");
            assert!(headers.contains(&key), "missing column {key}");
        }
    }

    #[test]
    fn natural_cmp_is_numeric_not_lexical() {
        use std::cmp::Ordering;
        assert_eq!(natural_cmp("x[2]", "x[10]"), Ordering::Less);
        assert_eq!(natural_cmp("x[9]", "x[10]"), Ordering::Less);
        assert_eq!(natural_cmp("x[99]", "x[100]"), Ordering::Less);
        // lexical sort would (wrongly) put "[10]" before "[2]"
    }
}
