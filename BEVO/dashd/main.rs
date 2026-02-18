use anyhow::{Context, Result};
use prost::Message; // Required for the .decode() method
use std::io::Read;
use std::os::unix::net::UnixStream;
use std::thread;
use std::time::Duration;

// --- Load Generated Proto ---
pub mod proto {
    include!(concat!(env!("OUT_DIR"), "/sensor_data.rs"));
}
use proto::SensorData;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";

fn main() -> Result<()> {
    loop {
        println!("[dashd] Connecting to {}...", SOCKET_PATH);

        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                println!("[dashd] Connected!");

                let mut buffer = [0u8; 4096];

                loop {
                    match stream.read(&mut buffer) {
                        Ok(0) => {
                            println!("[dashd] Server disconnected.");
                            break;
                        }
                        Ok(n) => {
                            match SensorData::decode(&buffer[..n]) {
                                Ok(data) => {
                                    println!("[dashd] Received: {:?}", data);
                                    
                                    if let Some(p) = &data.pack {
                                        println!(" >> Voltage: {}V", p.hv_pack_v);
                                    }
                                }
                                Err(e) => eprintln!("[dashd] Decode error: {}", e),
                            }
                        }
                        Err(e) => {
                            eprintln!("[dashd] Read error: {}", e);
                            break;
                        }
                    }
                }
            }
            Err(_) => thread::sleep(Duration::from_secs(2)),
        }
    }
}