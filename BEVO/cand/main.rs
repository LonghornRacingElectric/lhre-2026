use std::io::Write;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

const WAIT: u64 = 10;
const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";

fn main() -> std::io::Result<()> {
    // Remove old socket file if it exists
    if Path::new(SOCKET_PATH).exists() {
        std::fs::remove_file(SOCKET_PATH)?;
    }

    let listener = UnixListener::bind(SOCKET_PATH)?;
    println!("[CAND] Server listening at {}", SOCKET_PATH);

    let clients = Arc::new(Mutex::new(Vec::new()));

    // thread to accept clients
    let clients_clone = Arc::clone(&clients);
    thread::spawn(move || accept_clients(listener, clients_clone));

    // thread broadcasts messages
    broadcast_loop(clients);
}

// Accepts incoming Unix socket clients and stores them in the shared list
fn accept_clients(listener: UnixListener, clients: Arc<Mutex<Vec<UnixStream>>>) {
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                println!("[CAND] New client connected");
                clients.lock().expect("Mutex lock poisoned").push(stream);
            }
            Err(err) => eprintln!("[CAND] Connection error: {}", err),
        }
    }
}

// Broadcasts messages to all connected clients
fn broadcast_loop(clients: Arc<Mutex<Vec<UnixStream>>>) -> ! {
    let mut count = 0;
    loop {
        let message = format!("Broadcast message: {}\n", count);

        {
            let mut clients_guard = clients.lock().expect("Mutex lock poisoned");
            clients_guard.retain_mut(|stream| {
                if stream.write_all(message.as_bytes()).is_err() {
                    println!("[CAND] Client disconnected");
                    false
                } else {
                    true
                }
            });
        }

        count += 1;
        thread::sleep(Duration::from_millis(WAIT));
    }
}
