use anyhow::{Context, Result};
use prost::Message;
use std::io::Read;
use std::os::unix::net::UnixStream;
use std::thread;
use std::time::Duration;

use sensor_proto::proto::SensorData;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";

fn main() -> Result<()> {
    loop {
        println!("[DASHD] Connecting to {}...", SOCKET_PATH);

        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                println!("[DASHD] Connected!");
                let mut buffer = [0u8; 4096];

                loop {
                    match stream.read(&mut buffer) {
                        Ok(0) => {
                            println!("[DASHD] Server disconnected.");
                            break;
                        }
                        Ok(n) => {
                            match SensorData::decode(&buffer[..n]) {
                                Ok(data) => {
                                    
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
                                    if let Some(h) = &data.diagnostics_high {
                                        println!(" [DIAG_HIGH]: {:#?}", h);
                                    }
                                    if let Some(t) = &data.thermal {
                                        println!(" [THERMAL]: {:#?}", t);
                                    }
                                }
                                Err(e) => eprintln!("[DASHD] Decode error: {}", e),
                            }
                        }
                        Err(e) => {
                            eprintln!("[DASHD] Read error: {}", e);
                            break;
                        }
                    }
                }
            }
            Err(_) => {
                thread::sleep(Duration::from_secs(2));
            }
        }
    }
}