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
async fn run() -> anyhow::Result<()> {
    // Import the Socket trait here
    use socketcan::{CanFrame, CanSocket, Socket};
    use std::time::Duration;

    println!("Running in REAL mode: reading from CAN bus");

    async fn main() -> Result<()> {
        let mut sock_rx = CanSocket::open("vcan0")?;
        let sock_tx = CanSocket::open("can0")?;

        while let Some(Ok(frame)) = sock_rx.next().await {
            if matches!(frame, CanFrame::Data(_)) {
                sock_tx.write_frame(frame)?.await?;
            }
        }

        Ok(())
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
