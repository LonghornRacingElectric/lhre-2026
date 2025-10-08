use tokio::time::{sleep, Duration};
use zeromq::{SubSocket, Socket, SocketRecv};

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let mut socket = SubSocket::new();
    socket.connect("ipc:///tmp/vehicle_data.ipc").await?;

    loop {
        let msg = socket.recv().await?;  // this gives a single Bytes payload
        let data = msg.into_vec();          // convert to Vec<u8> if needed

        println!("Received: {:?}", data);

        sleep(Duration::from_millis(500)).await;
    }
}
