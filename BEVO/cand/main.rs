use clap::Parser;
use std::time::Duration;

/// Command-line options
#[derive(Parser)]
struct Args {
    #[arg(short, long, default_value = "real")]
    mode: String,
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    // match args.mode.as_str() {
    //     "real" => run_real(),
    //     "fake" => run_fake(),
    //     _ => anyhow::bail!("Invalid mode: use 'real' or 'fake'"),
    // }

    run();

    return { Ok(()) };
}

#[cfg(target_os = "linux")]
fn run() -> anyhow::Result<()> {
    use socketcan::{CanFrame, CanSocket};

    println!("Running in REAL mode: reading from CAN bus");

    let socket = CANSocket::open("can0")?;

    loop {
        match socket.read_frame() {
            Ok(frame) => {
                println!("[REAL] Got CAN frame: {:?}", frame);
                // TODO: Publish to message bus
            }
            Err(e) => eprintln!("[REAL] Read error: {}", e),
        }

        std::thread::sleep(Duration::from_millis(10));
    }
}

#[cfg(not(target_os = "linux"))]
fn run() -> anyhow::Result<()> {
    // use socketcan::CANFrame;

    println!("Running in FAKE mode: generating dummy CAN frames");

    let mut counter = 0;

    loop {
        let fake_data = [counter as u8, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78];
        // let fake_frame = CANFrame::new(0x123, &fake_data, false, false)?;

        // println!("[FAKE] Generated CAN frame: {:?}", fake_frame);
        // TODO: Publish to message bus

        counter = (counter + 1) % 256;
        std::thread::sleep(Duration::from_millis(500));
    }
}
