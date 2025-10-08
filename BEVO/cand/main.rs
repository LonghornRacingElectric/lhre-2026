use std::io::Write;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

const WAIT: u64 = 10;
const SOCKET_PATH: &str = "/tmp/main.sock";

fn main() -> std::io::Result<()> {
    // Remove old socket file if it exists
    if Path::new(SOCKET_PATH).exists() {
        std::fs::remove_file(SOCKET_PATH)?;
    }

    let listener = UnixListener::bind(SOCKET_PATH)?;
    println!("[CAND] Server listening at {}", SOCKET_PATH);

    let clients = Arc::new(Mutex::new(Vec::new()));

    // thread to accept clients
    let clients_accept = Arc::clone(&clients);
    thread::spawn(move || accept_clients(listener, clients_accept));

    // thread broadcasts messages
    broadcast_loop(clients);

    Ok(())
}

// Accepts incoming Unix socket clients and stores them in the shared list
fn accept_clients(listener: UnixListener, clients: Arc<Mutex<Vec<UnixStream>>>) {
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                println!("[CAND] New client connected");
                clients.lock().unwrap().push(stream);
            }
            Err(err) => eprintln!("[CAND] Connection error: {}", err),
        }
    }
}

// Broadcasts messages to all connected clients
fn broadcast_loop(clients: Arc<Mutex<Vec<UnixStream>>>) {
    let mut count = 0;
    loop {
        let message = format!("Broadcast message: {}\n", count);
        let mut dead_clients = Vec::new();

        let mut clients_guard = clients.lock().unwrap();
        for (i, stream) in clients_guard.iter_mut().enumerate() {
            if stream.write_all(message.as_bytes()).is_err() {
                println!("[CAND] Client disconnected");
                dead_clients.push(i);
            }
        }

        for &i in dead_clients.iter().rev() {
            clients_guard.remove(i);
        }

        count += 1;
        thread::sleep(Duration::from_millis(WAIT));
    }
}
