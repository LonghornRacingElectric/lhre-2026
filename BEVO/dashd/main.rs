use std::io::Read;
use std::os::unix::net::UnixStream;
use std::path::Path;
use std::thread;
use std::time::Duration;

const SOCKET_PATH: &str = "/tmp/BEVO_cand.sock";

fn main() {
    loop {
        println!(
            "[dashd] Attempting to connect to server at {}...",
            SOCKET_PATH
        );

        // Attempt to connect to the server's Unix socket.
        match UnixStream::connect(SOCKET_PATH) {
            Ok(mut stream) => {
                println!("[dashd] Connected successfully!");

                let mut buffer = [0; 1024];

                loop {
                    match stream.read(&mut buffer) {
                        Ok(bytes_read) => {
                            if bytes_read == 0 {
                                println!("[dashd] Server disconnected. Reconnecting...");
                                break;
                            }

                            let message = String::from_utf8_lossy(&buffer[..bytes_read]);
                            print!("[dashd] Received: {}", message);
                        }
                        Err(e) => {
                            eprintln!("[dashd] Connection error: {}. Reconnecting...", e);
                            break;
                        }
                    }
                }
            }
            Err(e) => {
                eprintln!("[dashd] Failed to connect: {}. Retrying in 2 seconds...", e);
            }
        }

        thread::sleep(Duration::from_secs(2));
    }
}
