use anyhow::Result;
use prost::Message;
use std::io::Read;
use std::os::unix::net::UnixStream;
use std::thread;
use std::time::Duration;

use sensor_proto::proto::orion::OrionSensorData;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";

fn main() -> Result<()> {
    loop {
        println!("[DASHD] Connecting to {}...", SOCKET_PATH);

        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                println!("[DASHD] Connected!");
                loop {
                    let mut length_buffer = [0u8; 4];
                    if stream.read_exact(&mut length_buffer).is_err() {
                        println!("[DASHD] Server disconnected.");
                        break;
                    }

                    let message_length = u32::from_be_bytes(length_buffer) as usize;
                    let mut message_buffer = vec![0u8; message_length];
                    if let Err(e) = stream.read_exact(&mut message_buffer) {
                        eprintln!("[DASHD] Read error: {}", e);
                        break;
                    }

                    match OrionSensorData::decode(&message_buffer[..]) {
                        Ok(data) => {
                            println!("[ORION_SENSOR_DATA] Received packet_id: {}, time: {}", data.packet_id, data.time);
                            if let Some(d) = &data.dynamics {
                                println!(" [DYNAMICS]: {:#?}", d);
                            }
                            if let Some(c) = &data.controls {
                                println!(" [CONTROLS]: {:#?}", c);
                            }
                            if let Some(p) = &data.pack {
                                println!(" [PACK]: {:#?}", p);
                            }
                            if let Some(l) = &data.diagnostics_low {
                                println!(" [DIAG_LOW]: {:#?}", l);
                            }
                            if let Some(t) = &data.thermal {
                                println!(" [THERMAL]: {:#?}", t);
                            }
                        }
                        Err(e) => eprintln!("[DASHD] Decode error: {}", e),
                    }
                }
            }
            Err(_) => {
                thread::sleep(Duration::from_secs(2));
            }
        }
    }
}