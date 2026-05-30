use anyhow::Result;
use prost::Message;
use sensor_proto::proto::orion::OrionSensorData;
use std::io::Read;
use std::os::unix::net::UnixStream;
use std::thread;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";

fn main() -> Result<()> {
    println!("[DEBUGD] Connecting to {}...", SOCKET_PATH);
    loop {
        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                println!("[DEBUGD] Connected to cand IPC");
                loop {
                    let mut len_buf = [0u8; 4];
                    if let Err(_) = stream.read_exact(&mut len_buf) {
                        println!("[DEBUGD] disconnected, retrying");
                        break;
                    }
                    let msg_len = u32::from_be_bytes(len_buf) as usize;
                    let mut msg_buf = vec![0u8; msg_len];
                    if let Err(e) = stream.read_exact(&mut msg_buf) {
                        eprintln!("[DEBUGD] read error: {}", e);
                        break;
                    }
                    match OrionSensorData::decode(&msg_buf[..]) {
                        Ok(data) => {
                            println!("[DEBUGD] packet_id={} time={}", data.packet_id, data.time);
                            if let Some(pack) = data.pack {
                                println!("  pack.hv_pack_v={} hv_soc={} dc_bus_current={}", pack.hv_pack_v, pack.hv_soc, pack.dc_bus_current);
                            }
                            if let Some(dyns) = data.dynamics {
                                println!("  dynamics.gps_speed={} accel_pedal_travel={}", dyns.gps_speed, dyns.accel_pedal_travel);
                            }
                            if let Some(therm) = data.thermal {
                                println!("  thermal.inverter_temp={} motor_temp={} ambient_temp={}", therm.inverter_temp, therm.motor_temp, therm.ambient_temp);
                            }
                            if let Some(ctrl) = data.controls {
                                println!("  controls.apps1_v={} apps2_v={} torque_request={}", ctrl.apps1_v, ctrl.apps2_v, ctrl.torque_request);
                            }
                        }
                        Err(e) => eprintln!("[DEBUGD] protobuf decode error: {}", e),
                    }
                }
            }
            Err(_) => {
                // cand not running yet; wait and retry
            }
        }
        thread::sleep(std::time::Duration::from_secs(2));
    }
}
