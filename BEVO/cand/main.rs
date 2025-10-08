use std::io::Write;
use std::os::unix::net::UnixListener;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

fn main() -> std::io::Result<()> {
    let socket_path = "/tmp/simple_stream.sock";

    if Path::new(socket_path).exists() {
        std::fs::remove_file(socket_path)?;
    }

    let listener = UnixListener::bind(socket_path)?;
    println!("[Publisher] Server listening for multiple clients at {}", socket_path);

    let clients = Arc::new(Mutex::new(Vec::new()));

    let clients_clone = clients.clone();
    thread::spawn(move || {
        for stream in listener.incoming() {
            match stream {
                Ok(stream) => {
                    println!("[Publisher] New listener connected!");
                    clients_clone.lock().unwrap().push(stream);
                }
                Err(err) => {
                    eprintln!("[Publisher] Error accepting connection: {}", err);
                    break;
                }
            }
        }
    });

    // 3. The main thread will now act as the publisher.
    let mut count = 0;
    loop {
        let message = format!("Broadcast message: {}\n", count);
        let mut dead_clients = Vec::new();

        // Lock the list of clients to safely send data to them.
        let mut clients_guard = clients.lock().unwrap();
        
        if !clients_guard.is_empty() {
             println!("[Publisher] Sending to {} listeners.", clients_guard.len());
        }

        // Iterate over each connected client.
        for (i, mut stream) in clients_guard.iter().enumerate() {
            // Try to write the message.
            if stream.write_all(message.as_bytes()).is_err() {
                // If the write fails, the client has disconnected.
                // We can't remove it now (we're borrowing), so we mark it for removal.
                println!("[Publisher] A listener disconnected. Marking for removal.");
                dead_clients.push(i);
            }
        }

        // Remove any disconnected clients from our list.
        // We iterate in reverse to avoid messing up the indices.
        for &index in dead_clients.iter().rev() {
            clients_guard.remove(index);
        }
        
        // The lock on `clients_guard` is released here automatically.

        count += 1;
        thread::sleep(Duration::from_millis(1));
    }
}